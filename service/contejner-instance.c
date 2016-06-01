/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#define _GNU_SOURCE
#include <error.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "contejner-instance.h"
#include "contejner-common.h"

#define CONTAINER_NAME_SZ 20
#define STACK_SIZE 1024 * 1024
#define STDOUT_BUF_SZ 1024
#define STDERR_BUF_SZ 1024

/* List of namesapces to unshare */
#define DEFAULT_UNSHARED_NAMESPACES     \
    CLONE_NEWNET                \
    | CLONE_NEWIPC              \
    | CLONE_NEWPID              \
    | CLONE_NEWUTS              \
    | CLONE_NEWNS               \
    | CLONE_NEWUSER

struct _ContejnerInstance
{
  GObject parent_instance;

  // instance variables for subclass go here
};

typedef struct _ContejnerInstancePrivate ContejnerInstancePrivate;

struct mount {
    char *path_in_host;
    char *path_in_container;
};

struct _ContejnerInstancePrivate {
    gint stderr_fd;
    gint stdout_fd;
    int id;
    char name[CONTAINER_NAME_SZ];
    char *command;
    char **command_args;
    char *rootfs_path;
    char *stack;
    int unshared_namespaces;
    GSList *mounts;
    char stdout_buf[STDOUT_BUF_SZ];
    char stderr_buf[STDERR_BUF_SZ];
    ContejnerInstanceStatus status;
    pid_t pid;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_STDERR,
    PROP_STDOUT,
    PROP_STATUS,
    PROP_LAST
};

static GParamSpec *obj_properties[PROP_LAST] = { NULL, };

#define CONTEJNER_INSTANCE_GET_PRIVATE(object)                           \
          (G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
                                       contejner_instance_get_type(),    \
                                       ContejnerInstancePrivate))

G_DEFINE_TYPE(ContejnerInstance,
              contejner_instance,
              G_TYPE_OBJECT)


static gboolean reaper(gpointer data)
{
    ContejnerInstance *self = CONTEJNER_INSTANCE(data);
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(self);

    int status = 0;
    pid_t child_pid = waitpid(priv->pid, &status, WNOHANG);
    if (child_pid > 0) {
        if WIFEXITED(status) {
            g_debug("Child exited with status: %d", WEXITSTATUS(status));
        }

        priv->status = CONTEJNER_INSTANCE_STATUS_STOPPED;
        g_object_notify_by_pspec(G_OBJECT(self),
                                 obj_properties[PROP_STATUS]);
    }

    return G_SOURCE_CONTINUE;
}

void log_func (const gchar *log_domain,
               GLogLevelFlags log_level,
               const gchar *message,
               gpointer user_data)
{
    ContejnerInstance *self = CONTEJNER_INSTANCE(user_data);
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(self);
    char *new_domain = g_strdup_printf("%s%s", priv->name,
                                       log_domain ? log_domain : "");
    g_log_default_handler(new_domain, log_level, message, NULL);
    g_free (new_domain);
}

static void contejner_instance_get_property (GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec)
{
    ContejnerInstance *self = CONTEJNER_INSTANCE (object);
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(self);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, priv->name);
            break;
        case PROP_STDERR: {
            int fd = dup(priv->stderr_fd);
            lseek(fd, 0, SEEK_SET);
            g_value_set_int(value, fd);
            break;
        } case PROP_STDOUT: {
            int fd = dup(priv->stdout_fd);
            lseek(fd, 0, SEEK_SET);
            g_value_set_int(value, fd);
            break;
        } case PROP_STATUS: {
            g_value_set_int(value, priv->status);
            break;
        } default: {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void contejner_instance_set_property (GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
    ContejnerInstance *self = CONTEJNER_INSTANCE(object);
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(self);

    switch (property_id) {
        case PROP_NAME: {
            const gchar *v = g_value_get_string(value);
            int strl = strlen(v);
            if (strl >= CONTAINER_NAME_SZ) {
                g_warning ("Container name too long");
            } else {
                memcpy(priv->name, v, strlen(v));
            }
            break;
        } default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);

    }
}

static void contejner_instance_init (ContejnerInstance *svc) {
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE (svc);

    g_log_set_default_handler(log_func, svc);

    priv->rootfs_path = "/";
    priv->command = NULL;
    priv->stack = g_malloc(STACK_SIZE);
    priv->unshared_namespaces = DEFAULT_UNSHARED_NAMESPACES;
}

static void contejner_instance_class_init (ContejnerInstanceClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    g_type_class_add_private(class, sizeof(ContejnerInstancePrivate));

    object_class->set_property = contejner_instance_set_property;
    object_class->get_property = contejner_instance_get_property;

    obj_properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Display name of this container",
                             NULL  /* default value */,
                             G_PARAM_READWRITE);

    obj_properties[PROP_STDERR] =
        g_param_spec_int ("stderr", "stderr fd", "stderr file descriptor",
                          -1, G_MAXINT,
                          0,
                          G_PARAM_READWRITE);

    obj_properties[PROP_STDOUT] =
        g_param_spec_int ("stdout", "stdout fd", "stout file descriptor",
                          -1, G_MAXINT,
                          0,
                          G_PARAM_READWRITE);

    obj_properties[PROP_STATUS] =
        g_param_spec_int ("status", "container status", "Container status",
                          0, CONTEJNER_INSTANCE_STATUS_LAST - 1,
                          0,
                          G_PARAM_READABLE);

    g_object_class_install_properties (object_class,
                                       PROP_LAST,
                                       obj_properties);

}

static int child_func (void *arg) {
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(arg);
    int status = 0;

    /* Set up stdout & stderr */
    if (close(STDOUT_FILENO)) { g_error ("Failed to close stdout in child"); }
    if (dup2(priv->stdout_fd, STDOUT_FILENO) == -1) {
        g_error ("Failed to dup stdout");
    }

    if (close(STDERR_FILENO)) { g_error ("Failed to close stderr in child"); }
    if (dup2(priv->stderr_fd, STDERR_FILENO) == -1) {
        g_error ("Failed to dup stderr");
    }

    /* Change the root directory */
    if ((status = chroot(priv->rootfs_path))) {
        perror("chroot");
        return status;
    }

    /* Execute command with namespace unshared */
    if ((status = execv(priv->command, priv->command_args))) {
        perror("exec");
        return status;
    }

    return status;
}

ContejnerInstance * contejner_instance_new (int id)
{
    ContejnerInstance *instance = g_object_new (CONTEJNER_TYPE_INSTANCE, NULL);
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE (instance);

    priv->status = CONTEJNER_INSTANCE_STATUS_CREATED;
    priv->id = id;

    gchar *stdout_fname = g_strdup_printf("/stdout-%d-%d",getpid(),id);
    gchar *stderr_fname = g_strdup_printf("/stderr-%d-%d",getpid(),id);

    priv->stdout_fd = shm_open(stdout_fname,
                               O_CREAT | O_RDWR | O_EXCL,
                               S_IRUSR | S_IWUSR);
    if (priv->stdout_fd == -1) {
        g_error("Failed to create stdout buffer for container");
    }
    priv->stderr_fd = shm_open(stderr_fname,
                               O_CREAT | O_RDWR | O_EXCL,
                               S_IRUSR | S_IWUSR);
    if (priv->stderr_fd == -1) {
        g_error("Failed to create stderr buffer for container");
    }

    g_free(stdout_fname);
    g_free(stderr_fname);

    g_snprintf(priv->name,CONTAINER_NAME_SZ,"Container %d", id);

    return instance;
}

void contejner_instance_run (ContejnerInstance *instance,
                           ContejnerInstanceRunCallback cb,
                           gpointer user_data)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    char *message = "OK";
    enum contejner_error_code error = CONTEJNER_OK;

    if (!priv->command || !priv->command_args) {
        message = "No command supplied";
        error = CONTEJNER_ERR_FAILED_TO_START;
        priv->status = CONTEJNER_INSTANCE_STATUS_STOPPED;
        goto contejner_instance_run_return;
    }

    pid_t child_pid = clone(child_func,
                           priv->stack + STACK_SIZE,
                           priv->unshared_namespaces | SIGCHLD,
                           instance);
    if (child_pid == -1) {
        message = "Error from clone() call";
        error = CONTEJNER_ERR_FAILED_TO_START;
        priv->status = CONTEJNER_INSTANCE_STATUS_STOPPED;
        goto contejner_instance_run_return;
    }

    priv->status = CONTEJNER_INSTANCE_STATUS_RUNNING;
    g_idle_add(reaper, instance);

contejner_instance_run_return:
        g_object_notify_by_pspec(G_OBJECT(instance),
                                 obj_properties[PROP_STATUS]);
        cb (instance, error, message, user_data);
}

int contejner_instance_get_id (const ContejnerInstance *instance)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    return priv->id;
}

gboolean contejner_instance_set_command (ContejnerInstance *instance,
                                         const gchar *command,
                                         const gchar **args)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    priv->command = g_strdup(command);
    if (!priv->command) {
        return FALSE;
    }

    priv->command_args = g_strdupv((char **)args);
    if (!priv->command_args) {
        return FALSE;
    }

    return TRUE;
}

gboolean contejner_instance_set_root(ContejnerInstance *instance,
                                     const GFile *path)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    if (priv->status == CONTEJNER_INSTANCE_STATUS_RUNNING) {
        g_warning ("Container already running");
        return FALSE;
    }

    if (g_file_query_exists((GFile *)path, NULL)) {
        priv->rootfs_path = g_file_get_path ((GFile *)path);
        return TRUE;
    } else {
        g_warning ("Path does not exist: %s", g_file_get_path ((GFile *)path));
    }

    return FALSE;
}

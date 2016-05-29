/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#define _GNU_SOURCE
#include "contejner-instance.h"
#include "contejner-common.h"
#include <error.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

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
};

#define CONTEJNER_INSTANCE_GET_PRIVATE(object)                           \
          (G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
                                       contejner_instance_get_type(),    \
                                       ContejnerInstancePrivate))

G_DEFINE_TYPE(ContejnerInstance,
              contejner_instance,
              G_TYPE_OBJECT)

static void contejner_instance_init (ContejnerInstance *svc) {
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE (svc);
    priv->rootfs_path = "/";
    priv->command = NULL;
    priv->stack = g_malloc(STACK_SIZE);
    priv->unshared_namespaces = DEFAULT_UNSHARED_NAMESPACES;
}

static void contejner_instance_class_init (ContejnerInstanceClass *class)
{
    g_type_class_add_private(class, sizeof(ContejnerInstancePrivate));
}

static int child_func (void *arg) {
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(arg);
    int status;

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
}

ContejnerInstance * contejner_instance_new (int id)
{
    ContejnerInstance *instance = g_object_new (CONTEJNER_TYPE_INSTANCE, NULL);
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE (instance);

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
        goto contejner_instance_run_return;
    }

    pid_t child_pid = clone(child_func,
                           priv->stack + STACK_SIZE,
                           priv->unshared_namespaces | SIGCHLD,
                           instance);
    if (child_pid == -1) {
        message = "Error from clone() call";
        error = CONTEJNER_ERR_FAILED_TO_START;
        goto contejner_instance_run_return;
    }

contejner_instance_run_return:
        cb (instance, error, message, user_data);
}

const gchar *contejner_instance_get_name (const ContejnerInstance *instance)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    return priv->name;
}

int contejner_instance_get_id (const ContejnerInstance *instance)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    return priv->id;
}

void contejner_instance_set_name (ContejnerInstance *instance,
                                  const gchar *name)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    g_snprintf(priv->name, CONTAINER_NAME_SZ, name);;
}

gboolean contejner_instance_set_command (ContejnerInstance *instance,
                                         const gchar *command,
                                         const gchar **args)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    priv->command = g_strdup(command);
    priv->command_args = g_strdupv((char **)args);
}

gint contejner_instance_get_stdout(const ContejnerInstance *instance)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    int fd = dup(priv->stdout_fd);
    lseek(fd, 0, SEEK_SET);
    return fd;
}

gint contejner_instance_get_stderr(const ContejnerInstance *instance)
{
    ContejnerInstancePrivate *priv = CONTEJNER_INSTANCE_GET_PRIVATE(instance);
    int fd = dup(priv->stderr_fd);
    lseek(fd, 0, SEEK_SET);
    return fd;
}

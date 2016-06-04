/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "contejner-instance-interface.h"
#include <gio/gunixfdlist.h>

struct _ContejnerInstanceInterface
{
  GDBusInterfaceSkeleton parent_instance;

  // instance variables for subclass go here
};

typedef struct _ContejnerInstanceInterfacePrivate ContejnerInstanceInterfacePrivate;

struct _ContejnerInstanceInterfacePrivate {
        GDBusNodeInfo *node_info;
        gchar *dbus_name;
        gchar *dbus_object_path;
        ContejnerInstance *container;
        GDBusConnection *connection;
};

#define CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(object)                           \
          (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                                       contejner_instance_interface_get_type(),    \
                                       ContejnerInstanceInterfacePrivate))

static void container_running_cb(ContejnerInstance *container,
                                 enum contejner_error_code error,
                                 const char *message,
                                 gpointer user_data)
{
    GDBusMethodInvocation *invocation =
        G_DBUS_METHOD_INVOCATION(((void **)user_data)[1]);

    GVariant *value = g_variant_new("(is)", error, message);
    g_variant_ref(value);
    g_dbus_method_invocation_return_value (invocation, value);
    g_variant_unref(value);
}

static void handle_Run(GDBusMethodInvocation *invocation,
                       ContejnerInstanceInterface *self,
                       ContejnerInstanceInterfacePrivate *priv)
{
    void *created_data[] = {(void *) self, (void *) invocation};
    contejner_instance_run(priv->container,
                           container_running_cb,
                           created_data);
}


static void handle_SetCommand(GVariant *parameters,
                              GDBusMethodInvocation *invocation,
                              ContejnerInstanceInterfacePrivate *priv)
{
    gchar *command = NULL;
    GVariantIter *iter = NULL;
    gchar *str = NULL;

    g_variant_get(parameters, "(sas)", &command, &iter);

    if (!iter) {
        g_error ("Failed to parse");
    }

    int num_args = g_variant_iter_n_children(iter)+2;

    char *args[num_args];

    args[0] = g_strdup(command);

    int i = 1;
    while (g_variant_iter_loop (iter, "s", &str)) {
        args[i++] = g_strdup(str);
    }
    args[i] = NULL;
    g_variant_iter_free (iter);

    contejner_instance_set_command(priv->container,
                                   command,
                                   (const gchar **)args);

    i = 0;
    for (i = 0; i < num_args - 1; i++) {
        g_free(args[i]);
    }

    g_dbus_method_invocation_return_value (invocation, NULL);
}
static void handle_Connect(GDBusMethodInvocation *invocation,
                           ContejnerInstanceInterfacePrivate *priv)
{
    int fds[2] = { 0 };
    GValue v = G_VALUE_INIT;
    g_value_init(&v, G_TYPE_INT);
    g_object_get_property(G_OBJECT(priv->container), "stderr", &v);
    fds[1] = g_value_get_int(&v);

    g_object_get_property(G_OBJECT(priv->container), "stdout", &v);
    fds[0] = g_value_get_int(&v);

    GUnixFDList *fd_list = g_unix_fd_list_new_from_array(fds, 2);
    g_dbus_method_invocation_return_value_with_unix_fd_list (invocation,
                                                             NULL,
                                                             fd_list);
}
static void handle_SetRoot(GVariant *parameters,
                           GDBusMethodInvocation *invocation,
                           ContejnerInstanceInterfacePrivate *priv)
{
        GValue status = G_VALUE_INIT;
        gchar *path = NULL;
        g_value_init(&status, G_TYPE_INT);
        const char *m = g_dbus_method_invocation_get_method_name (invocation);
        char *func = NULL, *error = NULL;

        g_object_get_property(G_OBJECT(priv->container), "status", &status);

        if (g_value_get_int(&status) == CONTEJNER_INSTANCE_STATUS_RUNNING) {
            func = g_strdup_printf("%s.Error.AlreadyRunning", m);
            error = "Container already running";
            goto setroot_error;
        }

        g_variant_get(parameters, "(s)", &path);

        GFile *f = g_file_new_for_path(path);
        gboolean ok = contejner_instance_set_root(priv->container, f);
        g_object_unref(f);

        if (ok) {
            g_dbus_method_invocation_return_value (invocation, NULL);
            return;
        } else {
            func = g_strdup_printf("%s.Error.BadRoot", m);
            error = "Path does not exist";
        }
setroot_error:
        g_dbus_method_invocation_return_dbus_error(invocation,
                                                   func,
                                                   error);
        g_free(func);
}
static void handle_Kill(GVariant *parameters,
                        GDBusMethodInvocation *invocation,
                        ContejnerInstanceInterfacePrivate *priv)
{
        int signal = 0;
        g_variant_get(parameters, "(i)", &signal);
        gboolean ret = contejner_instance_kill(priv->container, signal);
        if (ret) {
            g_dbus_method_invocation_return_value (invocation, NULL);
        } else {
            gchar *func = g_strdup_printf("%s.Error.KillFailed",
                        g_dbus_method_invocation_get_method_name(invocation));
            gchar *error = "Failed to send kill signal";
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                       func,
                                                       error);
            g_free(func);
        }
}

static void handle_Prepare(GDBusMethodInvocation *invocation,
                           ContejnerInstanceInterfacePrivate *priv)
{
        gboolean ret = contejner_instance_prepare(priv->container);
        if (ret) {
            g_dbus_method_invocation_return_value (invocation, NULL);
        } else {
            gchar *func = g_strdup_printf("%s.Error.KillFailed",
                        g_dbus_method_invocation_get_method_name(invocation));
            gchar *error = "Failed to send kill signal";
            g_dbus_method_invocation_return_dbus_error(invocation,
                                                       func,
                                                       error);
            g_free(func);
        }
}

static void dbus_method_call(G_GNUC_UNUSED GDBusConnection *connection,
                             G_GNUC_UNUSED const gchar *sender,
                             G_GNUC_UNUSED const gchar *object_path,
                             G_GNUC_UNUSED const gchar *interface_name,
                             const gchar *method_name,
                             GVariant *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
    ContejnerInstanceInterface *self = user_data;
    ContejnerInstanceInterfacePrivate *priv = CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(self);

    if (!g_strcmp0(method_name, "Run")) {
        handle_Run(invocation, self, priv);
    } else if (!g_strcmp0(method_name, "SetCommand")) {
        handle_SetCommand(parameters, invocation, priv);
    } else if (!g_strcmp0(method_name, "Connect")) {
        handle_Connect(invocation, priv);
    } else if (!g_strcmp0(method_name, "SetRoot")) {
        handle_SetRoot(parameters, invocation, priv);
    } else if (!g_strcmp0(method_name, "Kill")) {
        handle_Kill(parameters, invocation, priv);
    } else if (!g_strcmp0(method_name, "Prepare")) {
        handle_Prepare(invocation, priv);
    }
}

static GVariant *dbus_get_property (GDBusConnection *connection,
                             const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name,
                             GError **error,
                             gpointer user_data)
{
    return NULL;
}

static gboolean dbus_set_property (GDBusConnection *connection,
                             const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name,
                             GVariant *value,
                             GError **error,
                             gpointer user_data)
{
    return FALSE;
}

static GDBusInterfaceVTable dbus_interface_vtable = {
    dbus_method_call,
    dbus_get_property,
    dbus_set_property
};


G_DEFINE_TYPE(ContejnerInstanceInterface,
              contejner_instance_interface,
              G_TYPE_DBUS_INTERFACE_SKELETON)

static void flush (GDBusInterfaceSkeleton *skel) { /* No operation */ }
static void load_node_info(ContejnerInstanceInterface *svc) {
    ContejnerInstanceInterfacePrivate *priv =
        CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE (svc);

    gchar *introspection_xml = NULL;
    gsize introspection_xml_sz;
    GError *error = NULL;
    GDBusNodeInfo *node_info = NULL;
    GDBusInterfaceInfo *interface_info = NULL;

    if (!g_file_get_contents(CONTEJNER_INSTANCE_INTERFACE_XML,
                             &introspection_xml,
                             &introspection_xml_sz,
                             &error)) {
        g_error ("Failed to read introspection '%s'",
                 CONTEJNER_INSTANCE_INTERFACE_XML);
    }

    node_info = g_dbus_node_info_new_for_xml (introspection_xml, &error);
    if (!node_info) {
        g_error ("Failed to parse introspection '%s'",
                 CONTEJNER_INSTANCE_INTERFACE_XML);
    }

    interface_info = g_dbus_node_info_lookup_interface (
                                        node_info, CONTEJNER_INSTANCE_INTERFACE_NAME);
    if (!interface_info) {
        g_error ("Failed to find interface '%s' in file '%s'",
                 CONTEJNER_INSTANCE_INTERFACE_NAME,
                 priv->dbus_name);
    }

    g_free(interface_info->name);
    interface_info->name = priv->dbus_name;

    CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(svc)->node_info = node_info;
}

static GDBusInterfaceInfo *get_info (GDBusInterfaceSkeleton *skel)
{
    ContejnerInstanceInterfacePrivate *priv =
        CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(skel);
    return g_dbus_node_info_lookup_interface(priv->node_info,
                                             priv->dbus_name);
}

static GDBusInterfaceVTable *get_vtable (GDBusInterfaceSkeleton  *interface_)
{
    return &dbus_interface_vtable;
}

static GVariant *get_properties (GDBusInterfaceSkeleton  *interface_)
{
    GVariantDict dict;
    g_variant_dict_init (&dict, NULL);
    return g_variant_dict_end (&dict);
}

static void contejner_instance_interface_init (ContejnerInstanceInterface *svc) {
}

static void status_changed(GObject *instance,
                           GParamSpec* property,
                           gpointer user_data)
{
    ContejnerInstanceInterface *self = CONTEJNER_INSTANCE_INTERFACE(user_data);
    ContejnerInstanceInterfacePrivate *priv =
        CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(self);

    GValue new_value = G_VALUE_INIT;
    GVariant *variant = NULL;
    GError *error = NULL;

    g_value_init(&new_value, G_TYPE_INT);
    g_object_get_property(instance, "status", &new_value);
    ContejnerInstanceStatus status = g_value_get_int(&new_value);
    switch (status) {
        case CONTEJNER_INSTANCE_STATUS_RUNNING:
            variant = g_variant_new("(s)", "RUNNING");
            break;
        case CONTEJNER_INSTANCE_STATUS_STOPPED:
            variant = g_variant_new("(s)", "STOPPED");
            break;
        case CONTEJNER_INSTANCE_STATUS_CREATED:
            variant = g_variant_new("(s)", "CREATED");
            break;
        default: g_warning ("Illegal status received");
    }

    gboolean dbus_status =
        g_dbus_connection_emit_signal (priv->connection,
                                       NULL,
                                       priv->dbus_object_path,
                                       priv->dbus_name,
                                       "StatusChanged",
                                       variant,
                                       &error);
    if (!dbus_status) {
        g_warning("Failed to emit signal: %s", error->message);
    }

}

static void contejner_instance_interface_class_init (ContejnerInstanceInterfaceClass *class)
{
    g_type_class_add_private(class, sizeof(ContejnerInstanceInterfacePrivate));
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->flush = flush;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_info = get_info;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_vtable = get_vtable;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_properties = get_properties;
}

ContejnerInstanceInterface * contejner_instance_interface_new (ContejnerInstance *container,
                                                               GDBusConnection *connection)
{
   ContejnerInstanceInterface *svc = g_object_new (CONTEJNER_TYPE_INSTANCE_INTERFACE, NULL);
   ContejnerInstanceInterfacePrivate *priv =
       CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(svc);

   int id = contejner_instance_get_id(container);

   priv->dbus_name = g_strdup_printf("%s%d",
                                     CONTEJNER_INSTANCE_INTERFACE_NAME,
                                     id);
   priv->dbus_object_path = CONTEJNER_INSTANCE_INTERFACE_PATH;

   priv->connection = connection;
   priv->container = container;

   load_node_info(svc);

   g_signal_connect (container,
                    "notify::status",
                    G_CALLBACK(status_changed),
                    svc);

   return svc;
}

const gchar *contejner_instance_interface_get_dbus_interface (const ContejnerInstanceInterface *instance)
{
    ContejnerInstanceInterfacePrivate *priv =
        CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(instance);

    return priv->dbus_name;
}

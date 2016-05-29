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
        ContejnerInstance *container;
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

static void dbus_method_call(GDBusConnection *connection,
                              const gchar *sender,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *method_name,
                              GVariant *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
    ContejnerInstanceInterface *self = user_data;
    ContejnerInstanceInterfacePrivate *priv = CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(self);
    void *created_data[] = {(void *) self, (void *) invocation};

    if (!g_strcmp0(method_name, "Run")) {
        contejner_instance_run(priv->container,
                               container_running_cb,
                               created_data);
    } else if (!g_strcmp0(method_name, "SetCommand")) {
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
    } else if (!g_strcmp0(method_name, "Connect")) {
        int fds[2] = { 0 };
        fds[0] = contejner_instance_get_stdout(priv->container);
        fds[1] = contejner_instance_get_stderr(priv->container);

        GUnixFDList *fd_list = g_unix_fd_list_new_from_array(fds, 2);
        g_dbus_method_invocation_return_value_with_unix_fd_list (invocation,
                                                                 NULL,
                                                                 fd_list);
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

static void contejner_instance_interface_class_init (ContejnerInstanceInterfaceClass *class)
{
    g_type_class_add_private(class, sizeof(ContejnerInstanceInterfacePrivate));
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->flush = flush;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_info = get_info;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_vtable = get_vtable;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_properties = get_properties;
}

ContejnerInstanceInterface * contejner_instance_interface_new (ContejnerInstance *container)
{
   ContejnerInstanceInterface *svc = g_object_new (CONTEJNER_TYPE_INSTANCE_INTERFACE, NULL);
   ContejnerInstanceInterfacePrivate *priv =
       CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(svc);

   int id = contejner_instance_get_id(container);

   priv->dbus_name = g_strdup_printf("%s%d",
                                     CONTEJNER_INSTANCE_INTERFACE_NAME,
                                     id);
   priv->container = container;

   load_node_info(svc);

   return svc;
}

const gchar *contejner_instance_interface_get_dbus_interface (const ContejnerInstanceInterface *instance)
{
    ContejnerInstanceInterfacePrivate *priv =
        CONTEJNER_INSTANCE_INTERFACE_GET_PRIVATE(instance);

    return priv->dbus_name;
}

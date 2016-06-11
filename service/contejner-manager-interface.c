/*
 * Contejner - a d-bus interface to cgroups and namespaces
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "contejner-manager-interface.h"
#include "contejner-instance-interface.h"
#include "contejner-instance.h"
#include "dbus-service.xml.h"

struct _ContejnerManagerInterface
{
  GDBusInterfaceSkeleton parent_instance;

  // instance variables for subclass go here
};

typedef struct _ContejnerManagerInterfacePrivate ContejnerManagerInterfacePrivate;

struct _ContejnerManagerInterfacePrivate {
        GDBusNodeInfo *node_info;
        ContejnerManager *manager;
        GDBusObjectManagerServer *object_manager;
        GDBusObjectSkeleton *container_objects;
};

#define CONTEJNER_MANAGER_INTERFACE_GET_PRIVATE(object)                           \
          (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                                       contejner_manager_interface_get_type(),    \
                                       ContejnerManagerInterfacePrivate))

static void container_created_cb (ContejnerInstance *c, gpointer user_data)
{
    ContejnerManagerInterface *self = ((void**)user_data)[0];
    ContejnerManagerInterfacePrivate *priv = CONTEJNER_MANAGER_INTERFACE_GET_PRIVATE(self);
    GDBusMethodInvocation *m = ((void**)user_data)[1];
    GDBusConnection *connection =
        g_dbus_method_invocation_get_connection(m);

    ContejnerInstanceInterface *container_interface =
        contejner_instance_interface_new (c, connection);

    g_dbus_object_skeleton_add_interface(priv->container_objects,
                                         G_DBUS_INTERFACE_SKELETON(container_interface));

    const char *i =
        contejner_instance_interface_get_dbus_interface(container_interface);

    GVariant *value = g_variant_new("(s)", i);
    g_variant_ref(value);
    g_dbus_method_invocation_return_value (m, value);
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
    ContejnerManagerInterface *self = user_data;
    ContejnerManagerInterfacePrivate *priv = CONTEJNER_MANAGER_INTERFACE_GET_PRIVATE(self);
    void *created_data[] = {(void *) self, (void *) invocation};

    if (!g_strcmp0(method_name, "Create")) {
        contejner_manager_create(priv->manager,
                                 container_created_cb,
                                 created_data);
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


G_DEFINE_TYPE(ContejnerManagerInterface,
              contejner_manager_interface,
              G_TYPE_DBUS_INTERFACE_SKELETON)

static void flush (GDBusInterfaceSkeleton *skel) { /* No operation */ }
static void load_node_info(ContejnerManagerInterface *svc) {
    GError *error = NULL;
    GDBusNodeInfo *node_info = NULL;
    GDBusInterfaceInfo *interface_info = NULL;

    node_info = g_dbus_node_info_new_for_xml (CONTEJNER_MANAGER_INTERFACE_XML, &error);
    if (!node_info) {
        g_error ("Failed to parse introspection '%s'",
                 CONTEJNER_MANAGER_INTERFACE_XML);
    }

    interface_info = g_dbus_node_info_lookup_interface (
                            node_info, CONTEJNER_MANAGER_INTERFACE_DBUS_NAME);
    if (!interface_info) {
        g_error ("Failed to find interface '%s' in file '%s'",
                 CONTEJNER_MANAGER_INTERFACE_DBUS_NAME,
                 CONTEJNER_MANAGER_INTERFACE_XML);
    }

    CONTEJNER_MANAGER_INTERFACE_GET_PRIVATE(svc)->node_info = node_info;
}

static GDBusInterfaceInfo *get_info (GDBusInterfaceSkeleton *skel)
{
    return g_dbus_node_info_lookup_interface(
                    CONTEJNER_MANAGER_INTERFACE_GET_PRIVATE(skel)->node_info,
                    CONTEJNER_MANAGER_INTERFACE_DBUS_NAME);
}

static GDBusInterfaceVTable *get_vtable     (GDBusInterfaceSkeleton  *interface_)
{
    return &dbus_interface_vtable;
}

static GVariant *get_properties (GDBusInterfaceSkeleton  *interface_)
{
    return g_variant_new ("a{sv}");
}

static void contejner_manager_interface_init (ContejnerManagerInterface *svc) {
    load_node_info (svc);
}

static void contejner_manager_interface_class_init (ContejnerManagerInterfaceClass *class)
{
    g_type_class_add_private(class, sizeof(ContejnerManagerInterfacePrivate));
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->flush = flush;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_info = get_info;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_vtable = get_vtable;
    G_DBUS_INTERFACE_SKELETON_CLASS(class)->get_properties = get_properties;
}

ContejnerManagerInterface * contejner_manager_interface_new (ContejnerManager *cmgr,
                                          GDBusObjectManagerServer *mgr)
{
   ContejnerManagerInterface *svc = g_object_new (CONTEJNER_TYPE_MANAGER_INTERFACE, NULL);
   ContejnerManagerInterfacePrivate *priv =
       CONTEJNER_MANAGER_INTERFACE_GET_PRIVATE(svc);

   priv->manager = CONTEJNER_MANAGER (cmgr);
   priv->object_manager = G_DBUS_OBJECT_MANAGER_SERVER (mgr);
   priv->container_objects =
        g_dbus_object_skeleton_new("/org/jonatan/Contejner/Containers");
   g_dbus_object_manager_server_export(priv->object_manager,
                                       priv->container_objects);

   return svc;
}

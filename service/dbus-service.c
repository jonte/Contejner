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

#include <stdlib.h>

#include <gio/gio.h>

#include "contejner-manager-interface.h"
#include "contejner-manager.h"

static void on_bus_acquired (GDBusConnection *connection,
                             const gchar     *name,
                             gpointer         user_data)
{
    GDBusObjectManagerServer *object_manager;
    GError *error = NULL;
    ContejnerManagerInterface *svc = NULL;
    ContejnerManager *manager = user_data;

    object_manager = g_dbus_object_manager_server_new (CONTEJNER_MANAGER_INTERFACE_DBUS_PATH);
    g_dbus_object_manager_server_set_connection (object_manager, connection);

    svc = contejner_manager_interface_new(manager, object_manager);
    if (!svc) {
        g_error("Failed to allocate container manager interface");
    }

    if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(svc),
                                     connection,
                                     CONTEJNER_MANAGER_INTERFACE_DBUS_PATH,
                                     &error
                                     ))
    {
        g_error ("Failed to export interface");
    }
}

static void on_name_acquired (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer         user_data)
{
    g_debug ("Acquired the name %s on the session bus", name);
}

static void on_name_lost (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
    g_debug ("Lost the name %s on the session bus", name);
}

int main (int argc, char *argv[])
{
    guint owner_id;
    GMainLoop *loop;
    GBusNameOwnerFlags flags;
    gboolean opt_replace;
    gboolean opt_allow_replacement;
    GOptionContext *opt_context;
    GError *error;
    GOptionEntry opt_entries[] =
    {
        { "replace", 'r', 0, G_OPTION_ARG_NONE, &opt_replace, "Replace existing name if possible", NULL },
        { "allow-replacement", 'a', 0, G_OPTION_ARG_NONE, &opt_allow_replacement, "Allow replacement", NULL },
        { NULL}
    };
    ContejnerManager *manager;


    error = NULL;
    opt_replace = FALSE;
    opt_allow_replacement = FALSE;
    opt_context = g_option_context_new ("g_bus_own_name() example");
    g_option_context_add_main_entries (opt_context, opt_entries, NULL);
    if (!g_option_context_parse (opt_context, &argc, &argv, &error))
    {
        g_printerr ("Error parsing options: %s", error->message);
        return 1;
    }

    flags = G_BUS_NAME_OWNER_FLAGS_NONE;
    if (opt_replace)
        flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;
    if (opt_allow_replacement)
        flags |= G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;

    manager = contejner_manager_new();
    if (!manager) {
        g_error ("Failed to create container manager");
    }

    owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                               CONTEJNER_MANAGER_INTERFACE_DBUS_NAME,
                               flags,
                               on_bus_acquired,
                               on_name_acquired,
                               on_name_lost,
                               manager,
                               NULL);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    g_bus_unown_name (owner_id);

    return 0;
}

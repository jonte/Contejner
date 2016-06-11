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

#define _GNU_SOURCE
#include <stdio.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>

struct client {
    GDBusProxy *manager_proxy;
    GDBusProxy *container_proxy;
    GMainLoop *loop;
    gchar *exec_command;
    gchar **exec_command_args;
    gboolean do_list;
    gboolean do_create;
    gboolean do_connect;
    gchar *container_name;
    gint stdout_fd;
    gint stderr_fd;
    gint kill_signal;
};

static void print_output(const int *fds)
{
    static const char *fds_begin_text[] = {"\x1b[32m","\x1b[31m"};
    static const char *fds_end_text[] = {"\x1b[0m", "\x1b[0m"};
    char buf[1024] = { 0 };

    int i = 0;
    for (; fds[i] != -1; i++) {
        int fd = fds[i];
        printf("%s", fds_begin_text[i]);
        while(TRUE) {
            static const int READ_SZ = 1;
            int r = read(fd, buf, READ_SZ);
            if (r) {
                buf[r] = '\0';
                printf("%s", buf);
            }
            if (r != READ_SZ) {
                break;
            }
        }
        printf("%s", fds_end_text[i]);
        fflush(stdout);
    }
}

void status_change_cb (GDBusConnection *connection,
                       const gchar *sender_name,
                       const gchar *object_path,
                       const gchar *interface_name,
                       const gchar *signal_name,
                       GVariant *parameters,
                       gpointer user_data)
{
    struct client *client = user_data;
    const char *status;

    g_variant_get(parameters, "(s)", &status);
    g_debug ("Received new status: %s", status);
    if (!g_strcmp0("STOPPED", status)) {
        const int fds[] = {client->stdout_fd, client->stderr_fd, -1};
        print_output(fds);
        g_main_loop_quit(client->loop);
    }

}

static gboolean process_output(gpointer user_data)
{
    struct client *client = user_data;

    const int fds[] = {client->stdout_fd, client->stderr_fd, -1};

    print_output(fds);

    return G_SOURCE_CONTINUE;
}

static void connect (struct client *client)
{
    GError *error = NULL;
    gchar *command = g_strdup_printf("%s.Connect", client->container_name);
    GUnixFDList *fd_list = NULL;

    g_dbus_connection_signal_subscribe (
            g_dbus_proxy_get_connection(client->container_proxy),
            g_dbus_proxy_get_name(client->container_proxy),
            /* TODO: Why doesn't the below work? */
            NULL,//g_dbus_proxy_get_interface_name(client->container_proxy),
            "StatusChanged",
            g_dbus_proxy_get_object_path(client->container_proxy),
            NULL,
            G_DBUS_SIGNAL_FLAGS_NONE,
            status_change_cb,
            client,
            NULL);

    g_dbus_proxy_call_with_unix_fd_list_sync (client->container_proxy,
                                              command,
                                              NULL,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              &fd_list,
                                              NULL,
                                              &error);
    g_free (command);
    if (error) {
        g_error("Failed to call Connect");
    }

    if (g_unix_fd_list_get_length(fd_list) != 2) {
        g_error ("Received invalid fd list");
    }
    client->stdout_fd = g_unix_fd_list_get(fd_list, 0, &error);
    if (error) { g_error ("Failed to get file descriptor"); }

    client->stderr_fd = g_unix_fd_list_get(fd_list, 1, &error);
    if (error) { g_error ("Failed to get file descriptor"); }

    int flags = fcntl(client->stdout_fd, F_GETFL, 0);
    fcntl(client->stdout_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(client->stderr_fd, F_GETFL, 0);
    fcntl(client->stderr_fd, F_SETFL, flags | O_NONBLOCK);

    g_idle_add(process_output, client);
}

static gboolean open_container (struct client *client)
{
    GError *error = NULL;
    client->container_proxy =
        g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       "org.jonatan.Contejner",
                                       "/org/jonatan/Contejner/Containers",
                                       client->container_name,
                                       NULL,
                                       &error);
    if (error) {
        g_error("Failed to create proxy for %s", client->container_name);
        return FALSE;
    }

    return TRUE;
}

static char *create (struct client *client)
{
    GError *error = NULL;
    gchar *interface = NULL;

    GVariant *retval = g_dbus_proxy_call_sync (client->manager_proxy,
                                               "org.jonatan.Contejner.Create",
                                               NULL,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);
    if (retval) {
        g_variant_get (retval, "(s)", &interface);
        g_variant_unref (retval);
    } else if (error) {
        g_error("Failed to call Create");
    }

    return interface;
}

static void set_command (struct client *client)
{
    GError *error = NULL;
    gchar *command = g_strdup_printf("%s.SetCommand", client->container_name);
    GVariant *params = g_variant_new("(s^as)",
                                     client->exec_command,
                                     client->exec_command_args);

    g_dbus_proxy_call_sync (client->container_proxy,
                            command,
                            params,
                            G_DBUS_PROXY_FLAGS_NONE,
                            -1,
                            NULL,
                            &error);
    g_free (command);
    if (error) {
        g_error("Failed to call SetCommand");
    }
}

static void run (struct client *client)
{
    GError *error = NULL;
    gchar *command = g_strdup_printf("%s.Run", client->container_name);
    g_dbus_proxy_call_sync (client->container_proxy,
                            command,
                            NULL,
                            G_DBUS_PROXY_FLAGS_NONE,
                            -1,
                            NULL,
                            &error);
    g_free (command);
    if (error) {
        g_error("Failed to call Run");
    }
}

static void kill_ (struct client *client)
{
    GError *error = NULL;
    gchar *command = g_strdup_printf("%s.Kill", client->container_name);
    GVariant *params = g_variant_new("(i)", client->kill_signal);
    g_dbus_proxy_call_sync (client->container_proxy,
                            command,
                            params,
                            G_DBUS_PROXY_FLAGS_NONE,
                            -1,
                            NULL,
                            &error);
    g_free (command);
    if (error) {
        g_error("Failed to call Kill");
    }
}

static void list_containers(struct client *client)
{
    GError *error = NULL;
    GDBusObjectManager *mgr = g_dbus_object_manager_client_new_for_bus_sync (
                                     G_BUS_TYPE_SESSION,
                                     G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                     "org.jonatan.Contejner",
                                     "/org/jonatan/Contejner",
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &error);
    if (mgr) {
        GList *objects = g_dbus_object_manager_get_objects(mgr);
        GList *l;
        for (l = objects; l != NULL; l = l->next)
        {
            GDBusObject *o = G_DBUS_OBJECT(l->data);
            GList *ifaces = g_dbus_object_get_interfaces(o);
            GList *ll;
            for (ll = ifaces; ll != NULL; ll = ll->next)
            {
                GDBusInterface *interface = G_DBUS_INTERFACE(ll->data);
                const char *name = g_dbus_proxy_get_interface_name(G_DBUS_PROXY(interface));
                g_print(" - %s", name);
                g_object_unref(ll->data);
            }
            g_list_free(ifaces);
            g_object_unref(l->data);
        }
        g_list_free(objects);
    } else {
        g_print("Failed to list containers");
    }
}

static void proxy_ready (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
    GError *error = NULL;
    struct client *client = user_data;
    client->manager_proxy = g_dbus_proxy_new_for_bus_finish(res, &error);
    if (error) {
        g_error ("Failed to connect to Contejner service");
    }

    if (client->do_create) {
            gchar *interface = create(client);
            g_print ("Created new container: %s", interface);
            g_free (interface);
    }
    if (client->do_list) {
            list_containers(client);
    }
    if (client->exec_command) {
        if (!client->container_name) {
            client->container_name = create(client);
        }

        open_container(client);
        set_command(client);
        run(client);
    } if (client->kill_signal) {
        open_container(client);
        kill_(client);
    }

    /* Stop main loop if we are not connecting */
    if (client->do_connect) {
        open_container(client);
        connect (client);
    } else {
        g_main_loop_quit(client->loop);
    }
}

static void print_func (const gchar *string)
{
    printf("%s\n", string);
}

int main(int argc, char **argv) {
    GError *error = NULL;
    GOptionContext *context;
    struct client client = { 0 };
    gchar *command = NULL;
    gchar *container_name = NULL;

    GOptionEntry entries[] =
    {
        { "execute", 'e', 0, G_OPTION_ARG_STRING, &command, "Execute command in container", "CMD" },
        { "list", 'l', 0, G_OPTION_ARG_NONE, &client.do_list, "List available containers", NULL },
        { "new", 'n', 0, G_OPTION_ARG_NONE, &client.do_create, "Create new container", NULL },
        { "container", 'c', 0, G_OPTION_ARG_STRING, &container_name, "Container to operate on", NULL },
        { "connect-output", 'o', 0, G_OPTION_ARG_NONE, &client.do_connect, "Connect to stdout & stderr on container", NULL },
        { "kill", 'k', 0, G_OPTION_ARG_INT, &client.kill_signal, "Kill container with the supplied signal. Use integer value for signal. ", NULL },
        { NULL }
    };

    context = g_option_context_new ("- Contejner client");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        return 1;
    }

    if (client.kill_signal) {
        if (client.do_connect || client.do_create || client.do_list || command) {
            g_error("--kill must only be used together with --container");
        }
        if (!container_name) {
            g_error("--container is required when supplying --kill");
        }
    }

    if (command) {
        gchar **command_and_args = g_strsplit(command, " ", -1);
        client.exec_command = command_and_args[0];
        client.exec_command_args = command_and_args+1;
    }

    if (container_name) {
        gchar *prefix ="org.jonatan.Contejner.";
        if (!g_str_has_prefix(container_name, prefix)) {
            gchar *swap = g_strdup_printf("%s%s", prefix, container_name);
            g_free (container_name);
            container_name = swap;
        }
        client.container_name = container_name;
    }

    g_set_print_handler(print_func);

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_NONE,
                              NULL,
                              "org.jonatan.Contejner",
                              "/org/jonatan/Contejner",
                              "org.jonatan.Contejner",
                              NULL,
                              proxy_ready,
                              &client);

    client.loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (client.loop);
    return 0;
}

#include <stdio.h>
#include <gio/gio.h>

struct client {
    GDBusProxy *manager_proxy;
    GDBusProxy *container_proxy;
    GMainLoop *loop;
    gchar *exec_command;
    gchar **exec_command_args;
    gboolean do_list;
    gboolean do_create;
    gchar *container_name;
};

static gboolean open (struct client *client)
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
    g_print("Command is %s", command);
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
            g_print(g_dbus_object_get_object_path(o));
            GList *ifaces = g_dbus_object_get_interfaces(o);
            GList *ll;
            for (ll = ifaces; ll != NULL; ll = ll->next)
            {
                GDBusInterface *interface = G_DBUS_INTERFACE(ll->data);
                GDBusInterfaceInfo *info =
                    g_dbus_interface_get_info(interface);
                if (info) {
                    g_print(" - %s", info->name);
                } else {
                    g_print(" - Unable to get interface info");
                }
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

        open(client);
        set_command(client);
        run(client);
    }

    g_main_loop_quit(client->loop);
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
        { NULL }
    };

    context = g_option_context_new ("- Contejner client");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        return 1;
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

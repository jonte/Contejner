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

#ifndef CONTEJNER_INSTANCE_INTERFACE_H
#define CONTEJNER_INSTANCE_INTERFACE_H

#include <glib.h>
#include <gio/gio.h>

#include "contejner-common.h"


G_BEGIN_DECLS

#define CONTEJNER_TYPE_INSTANCE (contejner_instance_get_type ())
G_DECLARE_FINAL_TYPE(ContejnerInstance,
                     contejner_instance,
                     CONTEJNER,
                     INSTANCE, GObject)

typedef enum {
    CONTEJNER_INSTANCE_STATUS_RUNNING,
    CONTEJNER_INSTANCE_STATUS_STOPPED,
    CONTEJNER_INSTANCE_STATUS_CREATED,
    CONTEJNER_INSTANCE_STATUS_LAST,
} ContejnerInstanceStatus;

/* Callbacks */
typedef void (*ContejnerInstanceRunCallback)(ContejnerInstance *container,
                                             enum contejner_error_code,
                                             const char *message,
                                             gpointer user_data);


/* Functions */
ContejnerInstance *contejner_instance_new (int id);

void contejner_instance_run (ContejnerInstance *instance,
                             ContejnerInstanceRunCallback cb,
                             gpointer user_data);

int contejner_instance_get_id(const ContejnerInstance *instance);

gboolean contejner_instance_set_command(ContejnerInstance *instance,
                                        const gchar *command,
                                        const gchar **args);

gboolean contejner_instance_set_root(ContejnerInstance *instance,
                                     const GFile *path);

gboolean contejner_instance_kill(ContejnerInstance *instance, int signal);

G_END_DECLS

#endif /* DBUS_SERVICE_INTERFACE_H */

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

#ifndef CONTEJNER_INSTANCE_INTERFACE_INTERFACE_H
#define CONTEJNER_INSTANCE_INTERFACE_INTERFACE_H

#include <glib.h>
#include <gio/gio.h>
#include "contejner-manager.h"

G_BEGIN_DECLS

#define CONTEJNER_TYPE_INSTANCE_INTERFACE (contejner_instance_interface_get_type ())
G_DECLARE_FINAL_TYPE(ContejnerInstanceInterface,
                     contejner_instance_interface,
                     CONTEJNER,
                     INSTANCE_INTERFACE, GDBusInterfaceSkeleton)

ContejnerInstanceInterface *contejner_instance_interface_new (ContejnerInstance *intance,
                                                              GDBusConnection *connection);

const char *contejner_instance_interface_get_dbus_interface (const ContejnerInstanceInterface *i);

G_END_DECLS

#endif /* DBUS_INSTANCE_INTERFACE_INTERFACE_H */

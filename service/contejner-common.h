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

#ifndef CONTEJNER_COMMON_H
#define CONTEJNER_COMMON_H

#ifndef CONTEJNER_MANAGER_INTERFACE_DBUS_NAME
#define CONTEJNER_MANAGER_INTERFACE_DBUS_NAME "org.jonatan.Contejner"
#endif

#ifndef CONTEJNER_MANAGER_INTERFACE_DBUS_PATH
#define CONTEJNER_MANAGER_INTERFACE_DBUS_PATH "/org/jonatan/Contejner"
#endif

#ifndef CONTEJNER_INSTANCE_INTERFACE_NAME
#define CONTEJNER_INSTANCE_INTERFACE_NAME "org.jonatan.Contejner.Container"
#endif

#ifndef CONTEJNER_INSTANCE_INTERFACE_PATH
#define CONTEJNER_INSTANCE_INTERFACE_PATH "/org/jonatan/Contejner/Containers"
#endif

enum contejner_error_code {
    CONTEJNER_OK = 0,
    CONTEJNER_ERR_FAILED_TO_START,
    CONTEJNER_ERR_NO_SUCH_CONTAINER
};

#endif /* CONTEJNER_COMMON_H */

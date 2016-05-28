/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#ifndef CONTEJNER_MANAGER_INTERFACE_INTERFACE_H
#define CONTEJNER_MANAGER_INTERFACE_INTERFACE_H

#include <glib.h>
#include <gio/gio.h>
#include "contejner-manager.h"

G_BEGIN_DECLS

#define CONTEJNER_TYPE_MANAGER_INTERFACE (contejner_manager_interface_get_type ())
G_DECLARE_FINAL_TYPE(ContejnerManagerInterface,
                     contejner_manager_interface,
                     CONTEJNER,
                     MANAGER_INTERFACE, GDBusInterfaceSkeleton)

ContejnerManagerInterface *contejner_manager_interface_new (ContejnerManager *eng,
                                         GDBusObjectManagerServer *mgr);

G_END_DECLS

#endif /* DBUS_MANAGER_INTERFACE_INTERFACE_H */

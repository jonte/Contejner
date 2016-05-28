/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
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

ContejnerInstanceInterface *contejner_instance_interface_new (ContejnerInstance *intance);

const char *contejner_instance_interface_get_dbus_interface (const ContejnerInstanceInterface *i);

G_END_DECLS

#endif /* DBUS_INSTANCE_INTERFACE_INTERFACE_H */

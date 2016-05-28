/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#ifndef CONTEJNER_MANAGER_INTERFACE_H
#define CONTEJNER_MANAGER_INTERFACE_H

#include <glib.h>
#include <gio/gio.h>

#include "contejner-common.h"
#include "contejner-instance.h"

G_BEGIN_DECLS

#define CONTEJNER_TYPE_MANAGER (contejner_manager_get_type ())
G_DECLARE_FINAL_TYPE(ContejnerManager,
                     contejner_manager,
                     CONTEJNER,
                     MANAGER, GObject)

typedef void (*ContejnerManagerCreateCallback)(ContejnerInstance *c,
                                              gpointer user_data);


/**
 * Create a new ContainerManager
 */
ContejnerManager *contejner_manager_new (void);

/**
 * Create a new ContejnerInstance using the ContejnerManager
 */
void contejner_manager_create (ContejnerManager *manager,
                              ContejnerManagerCreateCallback cb,
                              gpointer user_data);

G_END_DECLS

#endif /* DBUS_SERVICE_INTERFACE_H */

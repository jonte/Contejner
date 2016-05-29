/*
 * Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#define _GNU_SOURCE
#include <error.h>
#include <stdio.h>
#include <sched.h>
#include "contejner-manager.h"
#include "contejner-instance.h"

#define CONTAINER_NAME_SZ 20
#define STACK_SIZE 1024 * 1024

/* List of namesapces to unshare */
#define DEFAULT_UNSHARED_NAMESPACES     \
    CLONE_NEWNET                \
    | CLONE_NEWIPC              \
    | CLONE_NEWPID              \
    | CLONE_NEWUTS              \
    | CLONE_NEWNS               \
    | CLONE_NEWUSER

struct _ContejnerManager
{
  GObject parent_instance;

  // instance variables for subclass go here
};

typedef struct _ContejnerManagerPrivate ContejnerManagerPrivate;

struct _ContejnerManagerPrivate {
    int next_container_id;
    GSList *container_list;
};

#define CONTEJNER_MANAGER_GET_PRIVATE(object)                           \
          (G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
                                       contejner_manager_get_type(),    \
                                       ContejnerManagerPrivate))

G_DEFINE_TYPE(ContejnerManager, contejner_manager, G_TYPE_OBJECT)

static void contejner_manager_init (ContejnerManager *svc) {
    ContejnerManagerPrivate *priv = CONTEJNER_MANAGER_GET_PRIVATE (svc);
    priv->next_container_id = 0;
}

static void contejner_manager_class_init (ContejnerManagerClass *class)
{
    g_type_class_add_private(class, sizeof(ContejnerManager));
}

ContejnerManager * contejner_manager_new (void)
{
  return g_object_new (CONTEJNER_TYPE_MANAGER, NULL);
}

void contejner_manager_create (ContejnerManager *manager,
                               ContejnerManagerCreateCallback cb,
                               gpointer user_data)
{
    ContejnerManagerPrivate *priv = CONTEJNER_MANAGER_GET_PRIVATE(manager);
    ContejnerInstance *container;
    int id = priv->next_container_id++;

    g_debug("Creating new container with ID: %d", id);
    container = contejner_instance_new(id);

    if (!container) {
        g_error ("Failed to allocate memory for container");
    }

    priv->container_list = g_slist_append(priv->container_list, container);

    cb (container, user_data);
}

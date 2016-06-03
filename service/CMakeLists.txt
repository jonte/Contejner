#  Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.

PROJECT (contejner)

CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(GLIB REQUIRED glib-2.0 gio-2.0 gio-unix-2.0)

SET (SOURCES
     dbus-service.c
     contejner-manager-interface.c
     contejner-manager.c
     contejner-instance.c
     contejner-instance-interface.c)

ADD_DEFINITIONS(-Wall -Werror)

INCLUDE_DIRECTORIES (${GLIB_INCLUDE_DIRS})

ADD_EXECUTABLE (contejner
    ${SOURCES})

TARGET_LINK_LIBRARIES (contejner
    ${GLIB_LIBRARIES}
    rt)

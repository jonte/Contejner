#  Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.

PROJECT (contejner)

CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(GLIB REQUIRED glib-2.0>=2.44 gio-2.0>=2.44 gio-unix-2.0>=2.44)

SET (SOURCES
     dbus-service.c
     contejner-manager-interface.c
     contejner-manager.c
     contejner-instance.c
     contejner-instance-interface.c)

ADD_CUSTOM_COMMAND(OUTPUT dbus-service.xml.h
                   COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/xml2h.sh CONTEJNER_MANAGER_INTERFACE_XML ${CMAKE_CURRENT_SOURCE_DIR}/dbus-service.xml > dbus-service.xml.h
                   DEPENDS dbus-service.xml)

ADD_CUSTOM_COMMAND(OUTPUT contejner-instance.xml.h
                   COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/xml2h.sh CONTEJNER_INSTANCE_INTERFACE_XML ${CMAKE_CURRENT_SOURCE_DIR}/contejner-instance.xml > contejner-instance.xml.h
                   DEPENDS contejner-instance.xml)

SET_SOURCE_FILES_PROPERTIES(contejner-manager-interface.c PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/dbus-service.xml.h")
SET_SOURCE_FILES_PROPERTIES(contejner-instance-interface.c PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/contejner-instance.xml.h")

ADD_DEFINITIONS(-Wall -Werror)

INCLUDE_DIRECTORIES (${GLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

ADD_EXECUTABLE (contejner
    ${SOURCES})

TARGET_LINK_LIBRARIES (contejner
    ${GLIB_LIBRARIES}
    rt)

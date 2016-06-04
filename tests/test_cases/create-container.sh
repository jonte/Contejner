#!/bin/bash
#  Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.

# Count containers before
NUM_PRE=$($INTROSPECT --object-path /org/jonatan/Contejner/Containers | fgrep org.jonatan.Contejner | wc -l)

# Create a new client
${CLIENT} -n

# Count containers after
NUM_POST=$($INTROSPECT --object-path /org/jonatan/Contejner/Containers | fgrep org.jonatan.Contejner | wc -l)

# Check that 1 container was created
ASSERT $((( ($NUM_PRE + 1) == $NUM_POST ))) "New container was not created"

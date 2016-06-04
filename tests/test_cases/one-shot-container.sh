#!/bin/bash
#  Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.

# Run 'echo' in a container
${CLIENT} -e "/bin/echo 'Hello world'" -o | grep --silent 'Hello world'
FOUND_OUTPUT=$?

# Check that the echo command was run
ASSERT_STREQUAL "$FOUND_OUTPUT" "0" "Failed to execute echo in container"

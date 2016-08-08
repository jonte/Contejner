#!/bin/bash
#  Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.

# Run 'echo' in a container
rm -f output
${CLIENT} -e "/bin/echo 'Hello world'" -o > output &
PID=$!
sleep 1
kill $PID
grep --silent 'Hello world' output
FOUND_OUTPUT=$?
rm -f output

# Check that the echo command was run
ASSERT_STREQUAL "$FOUND_OUTPUT" "0" "Failed to execute echo in container"

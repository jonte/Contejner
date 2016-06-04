#!/bin/bash
#  Copyright (C) 2016 Jonatan Pålsson <jonatan.p@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.

export CLIENT=${CLIENT_PREFIX}./contejner-client
export SERVICE=${SERVICE_PREFIX}./contejner

export INTROSPECT="gdbus introspect --session --dest org.jonatan.Contejner"

if [ ! -e "$SERVICE" ]; then
    echo "Could not find $SERVICE - export SERVICE_PREFIX to set a path to it"
    exit 1
fi

if [ ! -e "$CLIENT" ]; then
    echo "Could not find $CLIENT - export CLIENT_PREFIX to set a path to it"
    exit 1
fi

function ASSERT {
    truth="$1"
    message="$2"
    if [ "$1" != "1" ]; then
        echo "ASSERTION FAILED: $message"
        exit 1
    fi
}

function ASSERT_STREQUAL {
    str1="$1"
    str2="$2"
    message="$3"
    if [ "$1" != "$2" ]; then
        echo "ASSERTION FAILED: $message"
        exit 1
    fi
}

export -f ASSERT ASSERT_STREQUAL

# Start a new D-Bus
eval `dbus-launch --sh-syntax`

(G_MESSAGES_DEBUG=all ${SERVICE} &> contejner_output)&
SERVICE_PID=$!

# Give the service a second to start up
sleep 1

NUM_PASSED=0
NUM_FAILED=0
for test_case in test_cases/*.sh
do
    bash "$test_case"
    if [ "$?" == "0" ]; then
        NUM_PASSED=$((($NUM_PASSED + 1)))
    else

        NUM_FAILED=$((($NUM_FAILED + 1)))
    fi
done

# Kill the service
kill $SERVICE_PID

if [ "$NUM_FAILED" != "0" ]; then
    echo "========================== CONTAINER OUTPUT =========================="
    cat contejner_output
    echo "======================================================================"
fi

echo "============================ TEST RESULTS ============================"
echo "Number of passed tests: $NUM_PASSED"
echo "Number of failed tests: $NUM_FAILED"
echo "======================================================================"

if [ "$NUM_FAILED" != "0" ]; then
    exit 1
fi

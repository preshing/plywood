#!/bin/sh
#========================================================
#       ____
#      ╱   ╱╲    Plywood C++ Runtime Library
#     ╱___╱╭╮╲   https://plywood.dev/
#      └──┴┴┴┘
#========================================================

set -e

# Set PLYWOOD_ROOT to the normalized path to the script's parent directory.
PLYWOOD_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Get the app name.
if [ "$#" -eq 0 ]; then
    echo "Usage: ${0##*/} <app> [-run [app arguments...]]" >&2
    exit 1
fi
APP_NAME=$1
shift

# Accept optional -run argument after the app name.
RUN_APP=0
if [ "$#" -gt 0 ]; then
    if [ "$1" != "-run" ]; then
        echo "Usage: ${0##*/} <app> [-run [app arguments...]]" >&2
        exit 1
    fi
    RUN_APP=1
    shift
fi

# Reject names that don't identify an app project.
if [ ! -f "$PLYWOOD_ROOT/apps/$APP_NAME/CMakeLists.txt" ]; then
    echo "Error: '$APP_NAME' is not an app in $PLYWOOD_ROOT/apps." >&2
    exit 1
fi

# Generate the build system for the app (in Debug).
cmake -S "$PLYWOOD_ROOT/apps/$APP_NAME" -B "$PLYWOOD_ROOT/apps/$APP_NAME/build" -DCMAKE_BUILD_TYPE=Debug

# Build the app (in Debug).
cmake --build "$PLYWOOD_ROOT/apps/$APP_NAME/build" --config Debug

# Optionally run the app with all arguments following -run.
if [ "$RUN_APP" -eq 1 ]; then
    exec "$PLYWOOD_ROOT/bin/$APP_NAME" "$@"
fi

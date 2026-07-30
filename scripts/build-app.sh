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
if [ "$#" -gt 0 ] && [ "$1" != "-run" ]; then
    # Print usage.
    echo "Usage: ${0##*/} <app> [-run [app arguments...]]" >&2
    exit 1
fi

# Reject names that don't identify an app project.
if [ ! -f "$PLYWOOD_ROOT/apps/$APP_NAME/CMakeLists.txt" ]; then
    echo "Error: '$APP_NAME' is not an app in $PLYWOOD_ROOT/apps." >&2
    exit 1
fi

# Generate the build system for the app in Debug mode.
# Skip this step if -run was specified and CMakeCache.txt already exists.
BUILD_DIR="$PLYWOOD_ROOT/apps/$APP_NAME/build"
if [ "$1" != "-run" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cmake -S "$PLYWOOD_ROOT/apps/$APP_NAME" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
fi

# Build the app in Debug mode.
# This also regenerates the build system if out of date.
cmake --build "$BUILD_DIR" --config Debug

# Optionally run the app with all arguments following -run.
if [ "$1" = "-run" ]; then
    shift
    exec "$PLYWOOD_ROOT/bin/$APP_NAME" "$@"
fi

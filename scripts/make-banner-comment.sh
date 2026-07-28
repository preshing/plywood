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

# Generate the build system for the make-banner-comment app (in Debug).
cmake -S "$PLYWOOD_ROOT/apps/make-banner-comment" -B "$PLYWOOD_ROOT/apps/make-banner-comment/build" \
    -DCMAKE_BUILD_TYPE=Debug

# Build the make-banner-comment app (in Debug).
cmake --build "$PLYWOOD_ROOT/apps/make-banner-comment/build" --config Debug

# Run the make-banner-comment app.
exec "$PLYWOOD_ROOT/bin/make-banner-comment" "$@"

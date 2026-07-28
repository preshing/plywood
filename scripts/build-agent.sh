#!/bin/sh
#========================================================
#       ____
#      ╱   ╱╲    Plywood C++ Runtime Library
#     ╱___╱╭╮╲   https://plywood.dev/
#      └──┴┴┴┘
#========================================================

set -e

# Generate the build system for the agent app.
scriptDir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$scriptDir/../apps/agent" -B "$scriptDir/../apps/agent/build" -DCMAKE_BUILD_TYPE=Debug

# Perform a single build of the agent app.
cmake --build "$scriptDir/../apps/agent/build" --config Debug

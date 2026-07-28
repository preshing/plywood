#!/bin/sh
#========================================================
#       ____
#      ╱   ╱╲    Plywood C++ Runtime Library
#     ╱___╱╭╮╲   https://plywood.dev/
#      └──┴┴┴┘
#========================================================

set -e

# Print usage information when no command-line arguments are provided.
if [ "$#" -eq 0 ]; then
    echo "Usage: ${0##*/} [-b] <agent arguments>" >&2
    echo "-b : Build the agent app in Debug before running." >&2
    exec "$scriptDir/../apps/agent/build/agent"
    exit 1
fi

# Optionally build the agent app before running it.
scriptDir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$1" = "-b" ]; then
    "$scriptDir/build-agent.sh"
    shift
fi

# Run the agent app with all remaining arguments.
exec "$scriptDir/../apps/agent/build/agent" "$@"

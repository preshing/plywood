#!/bin/sh
#========================================================
#       ____
#      ╱   ╱╲    Plywood C++ Runtime Library
#     ╱___╱╭╮╲   https://plywood.dev/
#      └──┴┴┴┘
#========================================================

set -e

# Find the directory containing this script.
scriptDir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# Print usage information when no command-line options are provided.
if [ "$#" -eq 0 ]; then
    echo "Convenience script to build and run an agent in one command." >&2
    echo >&2
    echo "Usage: ${0##*/} [-b] <agent options>" >&2
    echo "  -b: Build the agent (in Debug) before running" >&2
    echo >&2
    echo "Options accepted by the agent:" >&2
    "$scriptDir/../apps/agent/build/agent" -usage
    exit 1
fi

# Optionally build the agent app before running it.
if [ "$1" = "-b" ]; then
    "$scriptDir/build-agent.sh"
    shift
fi

# Run the agent app with all remaining arguments.
exec "$scriptDir/../apps/agent/build/agent" "$@"

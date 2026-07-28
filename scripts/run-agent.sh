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

# Print usage information when no command-line options are provided.
if [ "$#" -eq 0 ]; then
    echo "Convenience script to build and run an agent in one command." >&2
    echo >&2
    echo "Usage: ${0##*/} [-b] <agent options>" >&2
    echo "  -b: Build the agent (in Debug) before running" >&2
    echo >&2
    echo "Options accepted by the agent:" >&2
    "$PLYWOOD_ROOT/bin/agent" -usage
    exit 1
fi

# Optionally build the agent app (in Debug) before running it.
if [ "$1" = "-b" ]; then
    "$PLYWOOD_ROOT/scripts/build-agent.sh"
    shift
fi

# Run the agent app with all remaining arguments.
exec "$PLYWOOD_ROOT/bin/agent" "$@"

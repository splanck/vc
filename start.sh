#!/bin/sh
set -e

# Build bundled libc required by the test suite
make libc

# Run the test suite by default
exec tests/run.sh "$@"

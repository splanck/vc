#!/bin/sh

# Common helper functions for test scripts

# Ensure the compiler exists, building it if necessary
ensure_compiler() {
    if [ ! -x "$BINARY" ]; then
        echo "Compiler not found, running make"
        (cd "$DIR/.." && make >/dev/null)
    fi
}

# Ensure internal libc archives are available
ensure_libc() {
    make -s -C "$DIR/../libc" >/dev/null
}

# Detect 32-bit compilation capability
check_can_compile_32() {
    set +e
    ${CC:-gcc} -m32 -xc /dev/null -o /dev/null 2>/dev/null
    CAN_COMPILE_32=$?
    set -e
}

# Create a temporary file or directory safely
safe_mktemp() {
    tmp=$(mktemp "$@") || {
        echo "mktemp failed" >&2
        return 1
    }
    if [ -z "$tmp" ]; then
        echo "mktemp failed" >&2
        return 1
    fi
    printf '%s' "$tmp"
}

# Compile a fixture and compare the generated assembly
# Usage: compile_fixture <cfile> <expected-asm> [extra compiler options]
compile_fixture() {
    cfile="$1"
    expect="$2"
    shift 2
    out=$(safe_mktemp) || return 1
    base=$(basename "$cfile" .c)
    echo "Running fixture $base"
    if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$expect"; then
        "$BINARY" "$@" -o "$out" "$cfile"
    else
        VC_NAMED_LOCALS=1 "$BINARY" "$@" -o "$out" "$cfile"
    fi
    if ! diff -u "$expect" "$out"; then
        echo "Test $base failed"
        fail=1
    fi
    rm -f "$out"
}


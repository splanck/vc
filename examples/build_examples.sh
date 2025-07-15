#!/bin/sh

# do not exit on first failure so all examples are attempted
DIR="$(dirname "$0")"
VC="$DIR/../vc"

# detect 32-bit compilation capability
gcc -m32 -xc /dev/null -o /dev/null 2>/dev/null
HAS_32=$?
ARCH_OPT=""
if [ $HAS_32 -ne 0 ]; then
    echo "32-bit compilation not available, building examples in 64-bit mode"
    ARCH_OPT="--x86-64"
fi

# build internal libc archives quietly
if ! make -s -C "$DIR/../libc"; then
    echo "Failed to build internal libc" >&2
    exit 1
fi

success=0
fail=0

for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    exe="$DIR/${base}"

    echo "Building $exe"

    "$VC" --link --internal-libc $ARCH_OPT -o "$exe" "$src" >"$exe.log" 2>&1
    if [ $? -eq 0 ]; then
        success=$((success + 1))
    else
        echo "Failed to build $exe (see $exe.log)"
        fail=$((fail + 1))
    fi
done

total=$((success + fail))
echo "Build summary: ${success} succeeded, ${fail} failed, ${total} total"

[ $fail -eq 0 ]

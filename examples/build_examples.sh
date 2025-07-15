#!/bin/sh
set -e
DIR="$(dirname "$0")"
VC="$DIR/../vc"

# detect 32-bit compilation capability
set +e
gcc -m32 -xc /dev/null -o /dev/null 2>/dev/null
HAS_32=$?
set -e
ARCH_OPT=""
if [ $HAS_32 -ne 0 ]; then
    echo "32-bit compilation not available, building examples in 64-bit mode"
    ARCH_OPT="--x86-64"
fi

for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    exe="$DIR/${base}"

    echo "Building $exe"

    "$VC" --link --internal-libc $ARCH_OPT -o "$exe" "$src"
done

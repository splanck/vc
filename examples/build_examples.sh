#!/bin/sh
set -e
DIR="$(dirname "$0")"
VC="$DIR/../vc"

for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    obj="$DIR/${base}.o"
    exe="$DIR/${base}"

    echo "Building $exe"
    "$VC" -c -o "$obj" "$src"
    cc -o "$exe" "$obj"
    rm -f "$obj"
done

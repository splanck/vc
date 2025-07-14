#!/bin/sh
set -e
DIR="$(dirname "$0")"
VC="$DIR/../vc"

for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    exe="$DIR/${base}"

    echo "Building $exe"

    "$VC" --link --internal-libc -o "$exe" "$src"
done

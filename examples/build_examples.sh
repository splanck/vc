#!/bin/sh
set -e
DIR="$(dirname "$0")"
VC="$DIR/../vc"

for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    exe="$DIR/${base}"

    echo "Building $exe"

    flags="--link --internal-libc"
    case "$base" in
        *_x86-64)
            flags="$flags --x86-64"
            ;;
    esac

    "$VC" $flags -o "$exe" "$src"
done

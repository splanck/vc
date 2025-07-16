#!/bin/sh
set -e
DIR="$(dirname "$0")"
EX_DIR="$DIR/../examples"
VC="$DIR/../vc"

# detect 32-bit compilation capability
set +e
gcc -m32 -xc /dev/null -o /dev/null 2>/dev/null
CAN_COMPILE_32=$?
set -e
ARCH_OPT=""
if [ $CAN_COMPILE_32 -ne 0 ]; then
    echo "32-bit compilation not available, using 64-bit mode"
    ARCH_OPT="--x86-64"
fi

# build internal libc quietly
if [ $CAN_COMPILE_32 -ne 0 ]; then
    # only build the 64-bit archive when 32-bit compilation is unavailable
    make -s -C "$DIR/../libc" libc64 >/dev/null
else
    make -s -C "$DIR/../libc" >/dev/null
fi

fail=0
for src in "$EX_DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    exe="$EX_DIR/$base"
    log="$exe.log"

    echo "Building $exe"
    set +e
    "$VC" --link $ARCH_OPT --internal-libc -o "$exe" "$src" >"$log" 2>&1
    status=$?
    set -e
    if [ $status -ne 0 ]; then
        echo "Build failed for $exe (see $log)"
        fail=1
        continue
    fi

    echo "Running $exe"
    set +e
    "$exe"
    status=$?
    set -e
    if [ $status -ne 0 ]; then
        echo "Execution failed for $exe"
        fail=1
    fi

done
exit $fail

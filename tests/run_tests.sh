#!/bin/sh
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"

fail=0
for cfile in "$DIR"/fixtures/*.c; do
    base=$(basename "$cfile" .c)
    expect="$DIR/fixtures/$base.s"
    out=$(mktemp)
    "$BINARY" -o "$out" "$cfile"
    if ! diff -u "$expect" "$out"; then
        echo "Test $base failed"
        fail=1
    fi
    rm -f "$out"
done

if [ $fail -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
fi
exit $fail

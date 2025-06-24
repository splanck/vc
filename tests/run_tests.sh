#!/bin/sh
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"

fail=0
for cfile in "$DIR"/fixtures/*.c; do
    base=$(basename "$cfile" .c)
    expect="$DIR/fixtures/$base.s"
    out=$(mktemp)
    echo "Running fixture $base"
    "$BINARY" -o "$out" "$cfile"
    if ! diff -u "$expect" "$out"; then
        echo "Test $base failed"
        fail=1
    fi
    rm -f "$out"
done

# negative test for parse error message
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "$out" "$DIR/invalid/parse_error.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Unexpected token" "$err" || ! grep -q "expected" "$err"; then
    echo "Test parse_error failed"
    fail=1
fi
rm -f "$out" "$err"

# test --dump-ir option
dump_out=$(mktemp)
"$BINARY" --dump-ir "$DIR/fixtures/simple_add.c" > "$dump_out"
if ! grep -q "movl \$7, %eax" "$dump_out"; then
    echo "Test dump_ir failed"
    fail=1
fi
rm -f "$dump_out"

if [ $fail -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
fi
exit $fail

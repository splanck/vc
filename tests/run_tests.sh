#!/bin/sh
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"

fail=0
for cfile in $(ls "$DIR"/fixtures/*.c | sort); do
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

# verify -O0 disables all optimizations for selected fixtures
for o0asm in "$DIR"/fixtures/*_O0.s; do
    base=$(basename "$o0asm" _O0.s)
    cfile="$DIR/fixtures/$base.c"
    out=$(mktemp)
    "$BINARY" -O0 -o "$out" "$cfile"
    if ! diff -u "$o0asm" "$out"; then
        echo "Test O0_$base failed"
        fail=1
    fi
    rm -f "$out"
done

# verify 64-bit code generation for selected fixtures
for asm64 in "$DIR"/fixtures/*_x86-64.s; do
    base=$(basename "$asm64" _x86-64.s)
    cfile="$DIR/fixtures/$base.c"
    out=$(mktemp)
    "$BINARY" --x86-64 -o "$out" "$cfile"
    if ! diff -u "$asm64" "$out"; then
        echo "Test x86_64_$base failed"
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

# negative test for undefined variable error message
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "$out" "$DIR/invalid/undef_var.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "$err"; then
    echo "Test undef_var failed"
    fail=1
fi
rm -f "$out" "$err"

# negative test for assigning to const variable
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "$out" "$DIR/invalid/const_assign.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "$err"; then
    echo "Test const_assign failed"
    fail=1
fi
rm -f "$out" "$err"

# test --dump-asm option
dump_out=$(mktemp)
"$BINARY" --dump-asm "$DIR/fixtures/simple_add.c" > "$dump_out"
if ! grep -q "movl \$7, %eax" "$dump_out"; then
    echo "Test dump_asm failed"
    fail=1
fi
rm -f "$dump_out"

# test --dump-ir option
ir_out=$(mktemp)
"$BINARY" --dump-ir "$DIR/fixtures/simple_add.c" > "$ir_out"
if ! grep -q "IR_CONST" "$ir_out"; then
    echo "Test dump_ir failed"
    fail=1
fi
rm -f "$ir_out"

# test --no-cprop option
cprop_out=$(mktemp)
"$BINARY" --no-cprop -o "$cprop_out" "$DIR/fixtures/const_load.c"
if ! grep -q "movl x, %eax" "$cprop_out"; then
    echo "Test no_cprop failed"
    fail=1
fi
rm -f "$cprop_out"

# test -c/--compile option
obj_out=$(mktemp --suffix=.o)
"$BINARY" -c -o "$obj_out" "$DIR/fixtures/simple_add.c"
if ! od -An -t x1 "$obj_out" | head -n 1 | grep -q "7f 45 4c 46"; then
    echo "Test compile_option failed"
    fail=1
fi
rm -f "$obj_out"

if [ $fail -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
fi
exit $fail

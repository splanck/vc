#!/bin/sh
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"

fail=0
for cfile in $(ls "$DIR"/fixtures/*.c | sort); do
    base=$(basename "$cfile" .c)

    case "$base" in
        *_x86-64|struct_*) continue;;
    esac
    case "$base" in
        include_search|include_angle|include_env) continue;;
    esac
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

# run struct fixtures separately to ensure struct member access and assignment
for cfile in "$DIR"/fixtures/struct_*.c; do
    [ -e "$cfile" ] || continue
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

# verify include search path option
inc_out=$(mktemp)
"$BINARY" -I "$DIR/includes" -o "$inc_out" "$DIR/fixtures/include_search.c"
if ! diff -u "$DIR/fixtures/include_search.s" "$inc_out"; then
    echo "Test include_search failed"
    fail=1
fi
rm -f "$inc_out"

# verify angle-bracket include search
angle_out=$(mktemp)
"$BINARY" -I "$DIR/includes" -o "$angle_out" "$DIR/fixtures/include_angle.c"
if ! diff -u "$DIR/fixtures/include_angle.s" "$angle_out"; then
    echo "Test include_angle failed"
    fail=1
fi
rm -f "$angle_out"

# verify VCPATH include search
env_out=$(mktemp)
VCPATH="$DIR/includes" "$BINARY" -o "$env_out" "$DIR/fixtures/include_env.c"
if ! diff -u "$DIR/fixtures/include_env.s" "$env_out"; then
    echo "Test include_env failed"
    fail=1
fi
rm -f "$env_out"

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

# negative test for const variable without initializer
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "$out" "$DIR/invalid/const_no_init.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "$err"; then
    echo "Test const_no_init failed"
    fail=1
fi
rm -f "$out" "$err"

# negative test for #error directive
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "$out" "$DIR/invalid/preproc_error.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Build stopped" "$err"; then
    echo "Test preproc_error failed"
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

# test -S option
dashS_out=$(mktemp)
"$BINARY" -S "$DIR/fixtures/simple_add.c" > "$dashS_out"
if ! grep -q "movl \$7, %eax" "$dashS_out"; then
    echo "Test dash_S failed"
    fail=1
fi
rm -f "$dashS_out"

# test --dump-ir option
ir_out=$(mktemp)
"$BINARY" --dump-ir "$DIR/fixtures/simple_add.c" > "$ir_out"
if ! grep -q "IR_CONST" "$ir_out"; then
    echo "Test dump_ir failed"
    fail=1
fi
rm -f "$ir_out"

# test -E/--preprocess option
pp_out=$(mktemp)
"$BINARY" -E "$DIR/fixtures/macro_object.c" > "$pp_out"
if ! grep -q "return 42;" "$pp_out"; then
    echo "Test preprocess_option failed"
    fail=1
fi
rm -f "$pp_out"

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

# test --link option
exe_out=$(mktemp)
"$BINARY" --x86-64 --link -o "$exe_out" "$DIR/fixtures/simple_add.c"
if ! od -An -t x1 "$exe_out" | head -n 1 | grep -q "7f 45 4c 46"; then
    echo "Test link_option failed"
    fail=1
fi
rm -f "$exe_out"

# test --std option
std_out=$(mktemp)
"$BINARY" --std=gnu99 -o "$std_out" "$DIR/fixtures/simple_add.c"
if ! diff -u "$DIR/fixtures/simple_add.s" "$std_out" > /dev/null; then
    echo "Test std_gnu99 failed"
    fail=1
fi
rm -f "$std_out"

err=$(mktemp)
set +e
"$BINARY" --std=c23 -o "$std_out" "$DIR/fixtures/simple_add.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Unknown standard" "$err"; then
    echo "Test invalid_std failed"
    fail=1
fi
rm -f "$std_out" "$err"

if [ $fail -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
fi
exit $fail

#!/bin/sh
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"

fail=0
# compile each fixture, including vla.c which exercises
# variable length array support
for cfile in "$DIR"/fixtures/*.c; do
    base=$(basename "$cfile" .c)

    case "$base" in
        *_x86-64|struct_*|bitfield_rw|include_search|include_angle|include_env|macro_bad_define|preproc_blank|macro_cli)
            continue;;
    esac
    expect="$DIR/fixtures/$base.s"
    out=$(mktemp)
    echo "Running fixture $base"
    "$BINARY" -o "${out}" "$cfile"
    if ! diff -u "$expect" "${out}"; then
        echo "Test $base failed"
        fail=1
    fi
    rm -f "${out}"
done

# run struct fixtures separately to ensure struct member access and assignment
for cfile in "$DIR"/fixtures/struct_*.c; do
    [ -e "$cfile" ] || continue
    base=$(basename "$cfile" .c)
    expect="$DIR/fixtures/$base.s"
    out=$(mktemp)
    echo "Running fixture $base"
    "$BINARY" -o "${out}" "$cfile"
    if ! diff -u "$expect" "${out}"; then
        echo "Test $base failed"
        fail=1
    fi
    rm -f "${out}"
done

# verify -O0 disables all optimizations for selected fixtures
for o0asm in "$DIR"/fixtures/*_O0.s; do
    base=$(basename "$o0asm" _O0.s)
    cfile="$DIR/fixtures/$base.c"
    out=$(mktemp)
    "$BINARY" -O0 -o "${out}" "$cfile"
    if ! diff -u "$o0asm" "${out}"; then
        echo "Test O0_$base failed"
        fail=1
    fi
    rm -f "${out}"
done

# verify 64-bit code generation for selected fixtures
for asm64 in "$DIR"/fixtures/*_x86-64.s; do
    base=$(basename "$asm64" _x86-64.s)
    cfile="$DIR/fixtures/$base.c"
    out=$(mktemp)
    "$BINARY" --x86-64 -o "${out}" "$cfile"
    if ! diff -u "$asm64" "${out}"; then
        echo "Test x86_64_$base failed"
        fail=1
    fi
    rm -f "${out}"
done

# verify Intel syntax assembly for simple_add.c
intel_out=$(mktemp)
"$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/simple_add.c"
if ! diff -u "$DIR/fixtures/simple_add_intel.s" "${intel_out}"; then
    echo "Test intel_simple_add failed"
    fail=1
fi
rm -f "${intel_out}"

# additional Intel syntax fixtures
intel_out=$(mktemp)
"$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/pointer_add.c"
if ! diff -u "$DIR/fixtures/pointer_add_intel.s" "${intel_out}"; then
    echo "Test intel_pointer_add failed"
    fail=1
fi
rm -f "${intel_out}"

intel_out=$(mktemp)
"$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/while_loop.c"
if ! diff -u "$DIR/fixtures/while_loop_intel.s" "${intel_out}"; then
    echo "Test intel_while_loop failed"
    fail=1
fi
rm -f "${intel_out}"

# verify include search path option
inc_out=$(mktemp)
"$BINARY" -I "$DIR/includes" -o "${inc_out}" "$DIR/fixtures/include_search.c"
if ! diff -u "$DIR/fixtures/include_search.s" "${inc_out}"; then
    echo "Test include_search failed"
    fail=1
fi
rm -f "${inc_out}"

# verify angle-bracket include search
angle_out=$(mktemp)
"$BINARY" -I "$DIR/includes" -o "${angle_out}" "$DIR/fixtures/include_angle.c"
if ! diff -u "$DIR/fixtures/include_angle.s" "${angle_out}"; then
    echo "Test include_angle failed"
    fail=1
fi
rm -f "${angle_out}"

# verify VCPATH include search
env_out=$(mktemp)
VCPATH="$DIR/includes" "$BINARY" -o "${env_out}" "$DIR/fixtures/include_env.c"
if ! diff -u "$DIR/fixtures/include_env.s" "${env_out}"; then
    echo "Test include_env failed"
    fail=1
fi
rm -f "${env_out}"

# verify VCINC include search
inc_env_out=$(mktemp)
VCINC="$DIR/includes" "$BINARY" -o "${inc_env_out}" "$DIR/fixtures/include_search.c"
if ! diff -u "$DIR/fixtures/include_search.s" "${inc_env_out}"; then
    echo "Test include_vcinc failed"
    fail=1
fi
rm -f "${inc_env_out}"

# verify command-line macro definitions
macro_out=$(mktemp)
"$BINARY" -DVAL=4 -DFLAG -o "${macro_out}" "$DIR/fixtures/macro_cli.c"
if ! diff -u "$DIR/fixtures/macro_cli.s" "${macro_out}"; then
    echo "Test define_option failed"
    fail=1
fi
rm -f "${macro_out}"

# verify command-line macro undefinition
macro_u_out=$(mktemp)
"$BINARY" -DFLAG -UFLAG -o "${macro_u_out}" "$DIR/fixtures/macro_cli_undef.c"
if ! diff -u "$DIR/fixtures/macro_cli_undef.s" "${macro_u_out}"; then
    echo "Test undef_option failed"
    fail=1
fi
rm -f "${macro_u_out}"

# negative test for parse error message
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/parse_error.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Unexpected token" "${err}" || ! grep -q "expected" "${err}"; then
    echo "Test parse_error failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for undefined variable error message
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/undef_var.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test undef_var failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for assigning to const variable
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/const_assign.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test const_assign failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for const variable without initializer
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/const_no_init.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test const_no_init failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for undefined variable in conditional expression
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/undef_cond.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test undef_cond failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for assignment to undefined variable
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/undef_assign.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test undef_assign failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for #error directive
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/preproc_error.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Build stopped" "${err}"; then
    echo "Test preproc_error failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for include cycle detection
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_cycle.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Include cycle detected" "${err}"; then
    echo "Test include_cycle failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for include cycle through search path
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -I "$DIR/includes" -o "${out}" "$DIR/invalid/include_cycle_search.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Include cycle detected" "${err}"; then
    echo "Test include_cycle_search failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for missing include file
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_missing.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "nonexistent.h: No such file or directory" "${err}"; then
    echo "Test include_missing failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for macro recursion limit
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/macro_cycle.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Macro expansion limit exceeded" "${err}"; then
    echo "Test macro_cycle failed"
    fail=1
fi
rm -f "${out}" "${err}"

# regression test for missing ')' in macro definition
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/macro_param_missing.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Missing ')' in macro definition" "${err}"; then
    echo "Test macro_param_missing failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for unterminated macro parameter list
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/macro_param_unterm.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Missing ')' in macro definition" "${err}"; then
    echo "Test macro_param_unterm failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for include depth limit
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_depth.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Include depth limit exceeded" "${err}"; then
    echo "Test include_depth failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for duplicate switch cases
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/duplicate_case.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test duplicate_case failed"
    fail=1
fi
rm -f "${out}" "${err}"

# test --dump-asm option
dump_out=$(mktemp)
"$BINARY" --dump-asm "$DIR/fixtures/simple_add.c" > "${dump_out}"
if ! grep -q "movl \$7, %eax" "${dump_out}"; then
    echo "Test dump_asm failed"
    fail=1
fi
rm -f "${dump_out}"

# test -S option
dashS_out=$(mktemp)
"$BINARY" -S "$DIR/fixtures/simple_add.c" > "${dashS_out}"
if ! grep -q "movl \$7, %eax" "${dashS_out}"; then
    echo "Test dash_S failed"
    fail=1
fi
rm -f "${dashS_out}"

# test --dump-ir option
ir_out=$(mktemp)
"$BINARY" --dump-ir "$DIR/fixtures/simple_add.c" > "${ir_out}"
if ! grep -q "IR_CONST" "${ir_out}"; then
    echo "Test dump_ir failed"
    fail=1
fi
rm -f "${ir_out}"

# test -E/--preprocess option
pp_out=$(mktemp)
"$BINARY" -E "$DIR/fixtures/macro_object.c" > "${pp_out}"
if ! grep -q "return 42;" "${pp_out}"; then
    echo "Test preprocess_option failed"
    fail=1
fi
rm -f "${pp_out}"

# verify blank lines are preserved by the preprocessor
pp_blank=$(mktemp)
"$BINARY" -E "$DIR/fixtures/preproc_blank.c" > "${pp_blank}"
if ! diff -u "$DIR/fixtures/preproc_blank.expected" "${pp_blank}"; then
    echo "Test preprocess_blank_lines failed"
    fail=1
fi
rm -f "${pp_blank}"

# simulate write failure with a full pipe
err=$(mktemp)
tmp_big=$(mktemp)
# create a large temporary source exceeding the pipe capacity
yes "int x = 0;" | head -n 7000 > "$tmp_big"
set +e
# use positional parameters so the compiler path can contain spaces
bash -c 'set -o pipefail; trap "" PIPE; "$1" -E "$2" 2> "$3" | head -c 1 >/dev/null' _ "$BINARY" "$tmp_big" "$err"
ret=$?
set -e
rm -f "$tmp_big"
if [ $ret -eq 0 ] || ! grep -qi "broken pipe" "$err"; then
    echo "Test preprocess_write_fail failed"
    fail=1
fi
rm -f "$err"

# test --no-cprop option
cprop_out=$(mktemp)
"$BINARY" --no-cprop -o "${cprop_out}" "$DIR/fixtures/const_load.c"
if ! grep -q "movl x, %eax" "${cprop_out}"; then
    echo "Test no_cprop failed"
    fail=1
fi
rm -f "${cprop_out}"

# test --no-inline option
inline_out=$(mktemp)
"$BINARY" --no-inline -o "${inline_out}" "$DIR/fixtures/inline_func.c"
if ! grep -q "call add" "${inline_out}"; then
    echo "Test no_inline failed"
    fail=1
fi
rm -f "${inline_out}"

# test --debug option
debug_out=$(mktemp)
"$BINARY" --debug -S "$DIR/fixtures/simple_add.c" > "${debug_out}"
if ! grep -q "\.file" "${debug_out}"; then
    echo "Test debug_option failed"
    fail=1
fi
rm -f "${debug_out}"

# test -c/--compile option
obj_tmp=$(mktemp tmp.XXXXXX)
obj_out="${obj_tmp}.o"
rm -f "${obj_tmp}"
"$BINARY" -c -o "${obj_out}" "$DIR/fixtures/simple_add.c"
if ! od -An -t x1 "${obj_out}" | head -n 1 | grep -q "7f 45 4c 46"; then
    echo "Test compile_option failed"
    fail=1
fi
rm -f "${obj_out}"

# test --intel-syntax --compile option (requires nasm)
if command -v nasm >/dev/null; then
    obj_tmp=$(mktemp tmp.XXXXXX)
    obj_out="${obj_tmp}.o"
    rm -f "${obj_tmp}"
    "$BINARY" --intel-syntax -c -o "${obj_out}" "$DIR/fixtures/simple_add.c"
    if ! od -An -t x1 "${obj_out}" | head -n 1 | grep -q "7f 45 4c 46"; then
        echo "Test compile_option_intel failed"
        fail=1
    fi
    rm -f "${obj_out}"
else
    echo "Skipping compile_option_intel (nasm not found)"
fi

# test --link option with spaces and semicolons in output path
link_tmpdir=$(mktemp -d)
exe_space="${link_tmpdir}/out with space"
"$BINARY" --x86-64 --link -o "${exe_space}" "$DIR/fixtures/simple_add.c"
if ! od -An -t x1 "${exe_space}" | head -n 1 | grep -q "7f 45 4c 46"; then
    echo "Test link_option_space failed"
    fail=1
fi
exe_semi="${link_tmpdir}/out;semi"
"$BINARY" --x86-64 --link -o "${exe_semi}" "$DIR/fixtures/simple_add.c"
if ! od -An -t x1 "${exe_semi}" | head -n 1 | grep -q "7f 45 4c 46"; then
    echo "Test link_option_semi failed"
    fail=1
fi
rm -f "${exe_space}" "${exe_semi}"
rmdir "${link_tmpdir}"

# test --std option
std_out=$(mktemp)
"$BINARY" --std=gnu99 -o "${std_out}" "$DIR/fixtures/simple_add.c"
if ! diff -u "$DIR/fixtures/simple_add.s" "${std_out}" > /dev/null; then
    echo "Test std_gnu99 failed"
    fail=1
fi
rm -f "${std_out}"

err=$(mktemp)
set +e
"$BINARY" --std=c23 -o "${std_out}" "$DIR/fixtures/simple_add.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Unknown standard" "${err}"; then
    echo "Test invalid_std failed"
    fail=1
fi
rm -f "${std_out}" "${err}"

# invalid optimization level should fail
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -O4 -o "${out}" "$DIR/fixtures/simple_add.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Invalid optimization level" "${err}"; then
    echo "Test invalid_opt_level failed"
    fail=1
fi
rm -f "${out}" "${err}"

# simulate disk full during assembly generation
lib="$DIR/libfail_fflush.so"
cc -shared -fPIC -o "$lib" "$DIR/unit/fail_fflush.c"
out=$(mktemp)
err=$(mktemp)
set +e
LD_PRELOAD="$lib" "$BINARY" -c -o "$out" "$DIR/fixtures/simple_add.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "No space left on device" "$err"; then
    echo "Test disk_full_flush failed"
    fail=1
fi
rm -f "$out" "$err" "$lib"

# regression test for long command error message
long_tmpdir=$(mktemp -d)
long_out="$long_tmpdir/$(printf 'a%.0s' {1..50})/$(printf 'b%.0s' {1..50})/$(printf 'c%.0s' {1..50})/$(printf 'd%.0s' {1..50})/$(printf 'e%.0s' {1..50})/out.o"
mkdir -p "$(dirname "$long_out")"
err=$(mktemp)
set +e
PATH=/nonexistent "$BINARY" -c -o "$long_out" "$DIR/fixtures/simple_add.c" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "$long_out" "$err"; then
    echo "Test long_command_error failed"
    fail=1
fi
rm -rf "$long_tmpdir" "$err"

# test reading source from stdin
stdin_out=$(mktemp)
cat "$DIR/fixtures/simple_add.c" | "$BINARY" -o "${stdin_out}" -
if ! diff -u "$DIR/fixtures/simple_add.s" "${stdin_out}" > /dev/null; then
    echo "Test stdin_source failed"
    fail=1
fi
rm -f "${stdin_out}"

# regression test for invalid source with --link (double free)
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" --link -o "${out}" "$DIR/invalid/parse_error.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || grep -qi "double free" "${err}"; then
    echo "Test link_invalid_double_free failed"
    fail=1
fi
rm -f "${out}" "${err}"

if [ $fail -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
fi
exit $fail

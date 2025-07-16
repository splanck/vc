#!/bin/sh
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"

# build the compiler if it's missing
if [ ! -x "$BINARY" ]; then
    echo "Compiler not found, running make"
    (cd "$DIR/.." && make >/dev/null)
fi

# check if the host compiler supports 32-bit builds
set +e
gcc -m32 -xc /dev/null -o /dev/null 2>/dev/null
CAN_COMPILE_32=$?
set -e
if [ $CAN_COMPILE_32 -ne 0 ]; then
    echo "Notice: 32-bit compilation not available, skipping 32-bit libc test"
fi

fail=0
# ensure internal libc archive is available for tests
make -s -C "$DIR/../libc" >/dev/null
# ensure host headers are not used
NO_SYSROOT="$DIR/empty_sysroot"
export VCFLAGS="--sysroot=$NO_SYSROOT"
# compile each fixture, including vla.c which exercises
# variable length array support
for cfile in "$DIR"/fixtures/*.c; do
    base=$(basename "$cfile" .c)

    case "$base" in
        *_x86-64|struct_*|bitfield_rw|include_search|include_angle|include_env|macro_bad_define|preproc_blank|macro_cli|macro_cli_quote|include_once|include_once_link|include_next|include_next_quote|libm_program|union_example|varargs_double|include_stdio|libc_puts|libc_printf|local_program|local_assign|libc_fileio)
            continue;;
    esac
    expect="$DIR/fixtures/$base.s"
    out=$(mktemp)
    echo "Running fixture $base"
    if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$expect"; then
        "$BINARY" -o "${out}" "$cfile"
    else
        VC_NAMED_LOCALS=1 "$BINARY" -o "${out}" "$cfile"
    fi
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
    if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$expect"; then
        "$BINARY" -o "${out}" "$cfile"
    else
        VC_NAMED_LOCALS=1 "$BINARY" -o "${out}" "$cfile"
    fi
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
    if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$o0asm"; then
        "$BINARY" -O0 -o "${out}" "$cfile"
    else
        VC_NAMED_LOCALS=1 "$BINARY" -O0 -o "${out}" "$cfile"
    fi
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
    if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$asm64"; then
        "$BINARY" --x86-64 -o "${out}" "$cfile"
    else
        VC_NAMED_LOCALS=1 "$BINARY" --x86-64 -o "${out}" "$cfile"
    fi
    if ! diff -u "$asm64" "${out}"; then
        echo "Test x86_64_$base failed"
        fail=1
    fi
    rm -f "${out}"
done

# verify Intel syntax assembly for simple_add.c
intel_out=$(mktemp)
if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$DIR/fixtures/simple_add_intel.s"; then
    "$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/simple_add.c"
else
    VC_NAMED_LOCALS=1 "$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/simple_add.c"
fi
if ! diff -u "$DIR/fixtures/simple_add_intel.s" "${intel_out}"; then
    echo "Test intel_simple_add failed"
    fail=1
fi
rm -f "${intel_out}"

# additional Intel syntax fixtures
intel_out=$(mktemp)
if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$DIR/fixtures/pointer_add_intel.s"; then
    "$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/pointer_add.c"
else
    VC_NAMED_LOCALS=1 "$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/pointer_add.c"
fi
if ! diff -u "$DIR/fixtures/pointer_add_intel.s" "${intel_out}"; then
    echo "Test intel_pointer_add failed"
    fail=1
fi
rm -f "${intel_out}"

intel_out=$(mktemp)
if grep -Eq '\-[0-9]+\(%(e|r)bp\)|\[(e|r)bp-' "$DIR/fixtures/while_loop_intel.s"; then
    "$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/while_loop.c"
else
    VC_NAMED_LOCALS=1 "$BINARY" --intel-syntax -o "${intel_out}" "$DIR/fixtures/while_loop.c"
fi
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

# verify CPATH include search
cpath_out=$(mktemp)
CPATH="$DIR/includes" "$BINARY" -o "${cpath_out}" "$DIR/fixtures/include_search.c"
if ! diff -u "$DIR/fixtures/include_search.s" "${cpath_out}"; then
    echo "Test include_cpath failed"
    fail=1
fi
rm -f "${cpath_out}"

# verify C_INCLUDE_PATH include search
cinc_out=$(mktemp)
C_INCLUDE_PATH="$DIR/includes" "$BINARY" -o "${cinc_out}" "$DIR/fixtures/include_env.c"
if ! diff -u "$DIR/fixtures/include_env.s" "${cinc_out}"; then
    echo "Test include_c_include_path failed"
    fail=1
fi
rm -f "${cinc_out}"

# verify verbose include resolution with internal libc
err=$(mktemp)
"$BINARY" --preprocess --verbose-includes --internal-libc "$DIR/fixtures/include_stdio.c" >/dev/null 2> "$err"
if ! grep -q "libc/include/stdio.h" "$err"; then
    echo "Test verbose_internal_stdio failed"
    fail=1
fi
rm -f "$err"

# verify VCFLAGS options are parsed
vcflags_out=$(mktemp)
VCFLAGS="--x86-64 --sysroot=$NO_SYSROOT" "$BINARY" -o "${vcflags_out}" "$DIR/fixtures/simple_add.c"
if ! diff -u "$DIR/fixtures/simple_add_x86-64.s" "${vcflags_out}"; then
    echo "Test vcflags_x86_64 failed"
    fail=1
fi
rm -f "${vcflags_out}"

vcflags_out=$(mktemp)
VCFLAGS="--intel-syntax --sysroot=$NO_SYSROOT" "$BINARY" -o "${vcflags_out}" "$DIR/fixtures/pointer_add.c"
if ! diff -u "$DIR/fixtures/pointer_add_intel.s" "${vcflags_out}"; then
    echo "Test vcflags_intel failed"
    fail=1
fi
rm -f "${vcflags_out}"

# verify #include_next directive
next_out=$(mktemp)
"$BINARY" -I "$DIR/include_next/dir1" -I "$DIR/include_next/dir2" -I "$DIR/include_next/dir3" -o "${next_out}" "$DIR/fixtures/include_next.c"
if ! diff -u "$DIR/fixtures/include_next.s" "${next_out}"; then
    echo "Test include_next failed"
    fail=1
fi
rm -f "${next_out}"

# verify quoted #include_next directive
next_quote_out=$(mktemp)
"$BINARY" -I "$DIR/include_next_quote/dir1" -I "$DIR/include_next_quote/dir2" -I "$DIR/include_next_quote/dir3" -o "${next_quote_out}" "$DIR/fixtures/include_next_quote.c"
if ! diff -u "$DIR/fixtures/include_next_quote.s" "${next_quote_out}"; then
    echo "Test include_next_quote failed"
    fail=1
fi
rm -f "${next_quote_out}"

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

# verify quoted macro values are unquoted
macro_quote=$(mktemp)
"$BINARY" -E "$DIR/fixtures/macro_cli_quote.c" '-DVAL="1 + 4"' > "${macro_quote}"
if ! diff -u "$DIR/fixtures/macro_cli_quote.expected" "${macro_quote}"; then
    echo "Test macro_cli_quote failed"
    fail=1
fi
rm -f "${macro_quote}"

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

# negative test for failing static assertion
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/static_assert_fail.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "failed" "${err}"; then
    echo "Test static_assert_fail failed"
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

# negative test for include cycle with symlinked header
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_cycle_symlink.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Include cycle detected" "${err}"; then
    echo "Test include_cycle_symlink failed"
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
if [ $ret -eq 0 ] || ! grep -q "nonexistent.h: No such file or directory" "${err}" \
   || ! grep -q "#include \"nonexistent.h\"" "${err}" \
   || ! grep -q "Searched directories:" "${err}"; then
    echo "Test include_missing failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for include_next missing file
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -I "$DIR/include_next/miss1" -I "$DIR/include_next/miss2" -o "${out}" "$DIR/invalid/include_next_missing.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "foo.h: No such file or directory" "${err}" \
   || ! grep -q "#include_next <foo.h>" "${err}" \
   || ! grep -q "Searched directories:" "${err}"; then
    echo "Test include_next_missing failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for quoted include_next missing file
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -I "$DIR/include_next_quote/miss1" -I "$DIR/include_next_quote/miss2" -o "${out}" "$DIR/invalid/include_next_quote_missing.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "foo.h: No such file or directory" "${err}" \
   || ! grep -q "#include_next \"foo.h\"" "${err}" \
   || ! grep -q "Searched directories:" "${err}"; then
    echo "Test include_next_quote_missing failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for malformed include line
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_malformed.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Malformed include directive" "${err}"; then
    echo "Test include_malformed failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for malformed include_next line
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_next_malformed.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Malformed include directive" "${err}"; then
    echo "Test include_next_malformed failed"
    fail=1
fi
rm -f "${out}" "${err}"

# negative test for include_next outside header
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/include_next_outside.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "#include_next is invalid outside a header" "${err}"; then
    echo "Test include_next_outside failed"
    fail=1
fi
rm -f "${out}" "${err}"

# macro recursion should fail without expansion limit error
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/macro_cycle.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || grep -q "Macro expansion limit exceeded" "${err}"; then
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

# negative test for duplicate macro parameter names
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/macro_param_dup.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Duplicate macro parameter name" "${err}"; then
    echo "Test macro_param_dup failed"
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
if [ $ret -eq 0 ] || ! grep -q "Include depth limit exceeded: .*depth20.h (depth 20)" "${err}"; then
    echo "Test include_depth failed"
    fail=1
fi
rm -f "${out}" "${err}"

# verify custom include depth option works
depth_ok=$(mktemp)
"$BINARY" -fmax-include-depth=22 -o "${depth_ok}" "$DIR/invalid/include_depth.c"
if ! diff -u "$DIR/fixtures/include_depth_ok.s" "${depth_ok}"; then
    echo "Test include_depth_override failed"
    fail=1
fi
rm -f "${depth_ok}"

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

# negative test for invalid union member access
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" -o "${out}" "$DIR/invalid/union_bad_access.c" 2> "${err}"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "Semantic error" "${err}"; then
    echo "Test union_bad_access failed"
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

# verify restrict pointers are marked in IR
ir_restrict=$(mktemp)
"$BINARY" --dump-ir "$DIR/fixtures/restrict_load.c" > "${ir_restrict}"
if ! grep -q "alias=" "${ir_restrict}" || ! grep -q "restrict" "${ir_restrict}"; then
    echo "Test dump_ir_restrict failed"
    fail=1
fi
rm -f "${ir_restrict}"

# verify multiple restrict parameters produce separate alias records
ir_restrict_multi=$(mktemp)
"$BINARY" --dump-ir "$DIR/fixtures/restrict_ptr.c" > "${ir_restrict_multi}"
if [ $(grep -c "restrict" "${ir_restrict_multi}") -lt 2 ]; then
    echo "Test dump_ir_restrict_ptr failed"
    fail=1
fi
rm -f "${ir_restrict_multi}"

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

# verify #pragma once prevents repeated includes
pp_once=$(mktemp)
"$BINARY" -I "$DIR/includes" -E "$DIR/fixtures/include_once.c" > "${pp_once}"
if ! diff -u "$DIR/fixtures/include_once.expected" "${pp_once}"; then
    echo "Test pragma_once failed"
    fail=1
fi
rm -f "${pp_once}"

# verify #pragma once with symlinked header
pp_link=$(mktemp)
"$BINARY" -I "$DIR/includes" -E "$DIR/fixtures/include_once_link.c" > "${pp_link}"
if ! diff -u "$DIR/fixtures/include_once_link.expected" "${pp_link}"; then
    echo "Test pragma_once_symlink failed"
    fail=1
fi
rm -f "${pp_link}"

# verify _Pragma handling in glibc headers does not hit expansion limit
if [ -f /usr/include/sys/cdefs.h ]; then
    header=/usr/include/sys/cdefs.h
elif multi=$(gcc -print-multiarch 2>/dev/null) && \
     [ -f /usr/include/$multi/sys/cdefs.h ]; then
    header=/usr/include/$multi/sys/cdefs.h
else
    header=""
fi
if [ -z "$header" ]; then
    echo "Skipping pragma_glibc (sys/cdefs.h not found)"
else
    tmp_pragma=$(mktemp)
    echo '#include <sys/cdefs.h>' > "$tmp_pragma"
    err=$(mktemp)
    set +e
    "$BINARY" -E "$tmp_pragma" > /dev/null 2> "$err"
    ret=$?
    set -e
    if [ $ret -ne 0 ]; then
        head_line=$(head -n 1 "$err")
        echo "Skipping pragma_glibc (preprocessing failed: $head_line)"
    elif grep -q "Macro expansion limit exceeded" "$err"; then
        echo "Test pragma_glibc failed"
        fail=1
    fi
    rm -f "$tmp_pragma" "$err"
fi

# verify macros inside #if are expanded
pp_gnu=$(mktemp)
"$BINARY" -E "$DIR/fixtures/ifexpr_header.h" > "$pp_gnu"
if grep -q "__BEGIN_DECLS" "$pp_gnu"; then
    echo "Test ifexpr_macro_expand failed"
    fail=1
fi
rm -f "$pp_gnu"

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

# verify additional inline fixtures
multi_out=$(mktemp)
"$BINARY" -o "${multi_out}" "$DIR/fixtures/inline_multi.c"
if ! diff -u "$DIR/fixtures/inline_multi.s" "${multi_out}"; then
    echo "Test inline_multi failed"
    fail=1
fi
rm -f "${multi_out}"

return_out=$(mktemp)
"$BINARY" -o "${return_out}" "$DIR/fixtures/inline_return.c"
if ! diff -u "$DIR/fixtures/inline_return.s" "${return_out}"; then
    echo "Test inline_return failed"
    fail=1
fi
rm -f "${return_out}"

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

# test --emit-dwarf option
obj_tmp=$(mktemp tmp.XXXXXX)
obj_out="${obj_tmp}.o"
rm -f "${obj_tmp}"
"$BINARY" --emit-dwarf -c -o "${obj_out}" "$DIR/fixtures/simple_add.c"
if ! objdump -h "${obj_out}" | grep -q ".debug_line"; then
    echo "Test emit_dwarf_line failed"
    fail=1
fi
if ! objdump -h "${obj_out}" | grep -q ".debug_info"; then
    echo "Test emit_dwarf_info failed"
    fail=1
fi
rm -f "${obj_out}"

# test --link option with spaces and semicolons in output path
link_tmpdir=$(mktemp -d)
trap 'rm -rf "$link_tmpdir"' EXIT
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

# link program against libm using -l and -L options
libm_exe=$(mktemp)
"$BINARY" --x86-64 --link -o "${libm_exe}" "$DIR/fixtures/libm_program.c" -L/usr/lib -lm
if ! od -An -t x1 "${libm_exe}" | head -n 1 | grep -q "7f 45 4c 46"; then
    echo "Test link_libm failed"
    fail=1
fi
rm -f "${libm_exe}"

# build and run simple program with internal libc (32-bit and 64-bit)
if [ $CAN_COMPILE_32 -eq 0 ]; then
    libc32=$(mktemp)
    rm -f "${libc32}"
    "$BINARY" --link --internal-libc -o "${libc32}" "$DIR/fixtures/libc_puts.c"
    out=$("${libc32}")
    status=$?
    if [ "$out" != "hello" ] || [ $status -ne 0 ]; then
        echo "Test libc_puts_32 failed"
        fail=1
    fi
    rm -f "${libc32}"
fi

libc64=$(mktemp)
rm -f "${libc64}"
"$BINARY" --x86-64 --link --internal-libc -o "${libc64}" "$DIR/fixtures/libc_puts.c"
out=$("${libc64}")
status=$?
if [ "$out" != "hello" ] || [ $status -ne 0 ]; then
    echo "Test libc_puts_64 failed"
    fail=1
fi
rm -f "${libc64}"

if [ $CAN_COMPILE_32 -eq 0 ]; then
    libc_printf32=$(mktemp)
    rm -f "${libc_printf32}"
    "$BINARY" --link --internal-libc -o "${libc_printf32}" "$DIR/fixtures/libc_printf.c"
    if [ "$("${libc_printf32}")" != "hi" ]; then
        echo "Test libc_printf_32 failed"
        fail=1
    fi
    rm -f "${libc_printf32}"
fi

libc_printf64=$(mktemp)
rm -f "${libc_printf64}"
"$BINARY" --x86-64 --link --internal-libc -o "${libc_printf64}" "$DIR/fixtures/libc_printf.c"
if [ "$("${libc_printf64}")" != "hi" ]; then
    echo "Test libc_printf_64 failed"
    fail=1
fi
rm -f "${libc_printf64}"

# verify error message when internal libc archive is missing
archive_path="$DIR/../libc/libc64.a"
mv "$archive_path" "$archive_path.bak"
err=$(mktemp)
out=$(mktemp)
set +e
"$BINARY" --x86-64 --internal-libc -o "${out}" "$DIR/fixtures/simple_add.c" 2> "$err"
ret=$?
set -e
mv "$archive_path.bak" "$archive_path"
if [ $ret -eq 0 ] || ! grep -q "make libc64" "$err"; then
    echo "Test internal_libc_missing failed"
    fail=1
fi
rm -f "${out}" "${err}"

# build and run program with locals using internal libc
expected="5 + 2 = 7\n5 - 2 = 3\n5 * 2 = 10\n5 / 2 = 2\nSum 1..10 = 55"
if [ $CAN_COMPILE_32 -eq 0 ]; then
    local32=$(mktemp)
    rm -f "${local32}"
    "$BINARY" --link --internal-libc -o "${local32}" "$DIR/fixtures/local_program.c"
    if [ "$("${local32}")" != "$expected" ]; then
        echo "Test local_program_32 failed"
        fail=1
    fi
    rm -f "${local32}"
fi

local64=$(mktemp)
rm -f "${local64}"
"$BINARY" --x86-64 --link --internal-libc -o "${local64}" "$DIR/fixtures/local_program.c"
if [ "$("${local64}")" != "$expected" ]; then
    echo "Test local_program_64 failed"
    fail=1
fi
rm -f "${local64}"

# verify stack offsets in generated assembly for local_program
asm_chk=$(mktemp)
"$BINARY" --x86-64 --internal-libc -o "${asm_chk}" "$DIR/fixtures/local_program.c"
if grep -q "\bsum\b" "${asm_chk}"; then
    echo "Test local_program_stack failed"
    fail=1
fi
rm -f "${asm_chk}"

# verify assembly for local_assign with internal libc
assign_out=$(mktemp)
"$BINARY" --x86-64 --internal-libc -o "${assign_out}" "$DIR/fixtures/local_assign.c"
if ! diff -u "$DIR/fixtures/local_assign.s" "${assign_out}"; then
    echo "Test local_assign_libc failed"
    fail=1
fi
rm -f "${assign_out}"

# build and run simple file I/O program with internal libc
io_file="$DIR/input.txt"
echo "hello" > "$io_file"
if [ $CAN_COMPILE_32 -eq 0 ]; then
    fileio32=$(mktemp)
    rm -f "${fileio32}"
    "$BINARY" --link --internal-libc -o "${fileio32}" "$DIR/fixtures/libc_fileio.c"
    if [ "$("${fileio32}")" != "hello" ]; then
        echo "Test libc_fileio_32 failed"
        fail=1
    fi
    rm -f "${fileio32}"
fi

fileio64=$(mktemp)
rm -f "${fileio64}"
"$BINARY" --x86-64 --link --internal-libc -o "${fileio64}" "$DIR/fixtures/libc_fileio.c"
if [ "$("${fileio64}")" != "hello" ]; then
    echo "Test libc_fileio_64 failed"
    fail=1
fi
rm -f "${fileio64}" "$io_file"

# dependency generation with -MD
dep_obj=depobj$$.o
"$BINARY" -MD -c -I "$DIR/includes" -o "$dep_obj" "$DIR/fixtures/include_search.c"
dep_file="${dep_obj%.o}.d"
if [ ! -f "$dep_file" ] || ! grep -q "val.h" "$dep_file"; then
    echo "Test dep_md failed"
    fail=1
fi
rm -f "$dep_obj" "$dep_file"

# dependency generation with -M
dep_src="include_search.d"
rm -f "$dep_src"
"$BINARY" -M -I "$DIR/includes" "$DIR/fixtures/include_search.c"
if [ ! -f "$dep_src" ] || ! grep -q "val.h" "$dep_src"; then
    echo "Test dep_M failed"
    fail=1
fi
rm -f "$dep_src"

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

# simulate disk full during macro write
if command -v nasm >/dev/null; then
    lib="$DIR/libfail_fputs.so"
    cc -shared -fPIC -o "$lib" "$DIR/unit/fail_fputs.c"
    out=$(mktemp)
    err=$(mktemp)
    set +e
    LD_PRELOAD="$lib" "$BINARY" --intel-syntax -c -o "$out" "$DIR/fixtures/simple_add.c" 2> "$err"
    ret=$?
    set -e
    if [ $ret -eq 0 ] || ! grep -q "No space left on device" "$err"; then
        echo "Test disk_full_fputs failed"
        fail=1
    fi
    rm -f "$out" "$err" "$lib"
else
    echo "Skipping disk_full_fputs (nasm not found)"
fi

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

# unreachable warning for return
err=$(mktemp)
out=$(mktemp)
"$BINARY" -o "${out}" "$DIR/invalid/unreachable_return.c" 2> "${err}"
if ! grep -q "warning: unreachable statement" "${err}"; then
    echo "Test warn_unreachable_return failed"
    fail=1
fi
rm -f "${out}" "${err}"

# unreachable warning for goto
err=$(mktemp)
out=$(mktemp)
"$BINARY" -o "${out}" "$DIR/invalid/unreachable_goto.c" 2> "${err}"
if ! grep -q "warning: unreachable statement" "${err}"; then
    echo "Test warn_unreachable_goto failed"
    fail=1
fi
rm -f "${out}" "${err}"

# build and run example programs with internal libc
if ! "$DIR/run_examples.sh"; then
    echo "Test run_examples failed"
    fail=1
fi

if [ $fail -eq 0 ]; then
    echo "All tests passed"
else
    echo "Some tests failed"
fi
exit $fail

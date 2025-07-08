#!/bin/sh
# exit on unhandled errors
set -e
DIR=$(dirname "$0")
# track failing regression tests
fail=0
# build the compiler
make
# build unit test binary
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/unit_tests" "$DIR/unit/test_lexer_parser.c" \
    src/parser_core.c src/parser_init.c src/parser_decl.c \
    src/parser_flow.c src/parser_toplevel.c src/parser_expr.c \
    src/parser_stmt.c src/parser_types.c src/symtable_core.c \
    src/symtable_globals.c src/symtable_struct.c src/ast_clone.c \
    src/ast_expr.c src/ast_stmt.c src/lexer.c src/util.c \
    src/vector.c src/error.c
# build cli unit test binary with vector_push wrapper
cc -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -c src/cli.c -o cli_test.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_cli.c" -o "$DIR/test_cli.o"
cc -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_test.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_test.o
cc -o "$DIR/cli_tests" cli_test.o "$DIR/test_cli.o" vector_test.o util_test.o
rm -f cli_test.o "$DIR/test_cli.o" vector_test.o util_test.o
# build parser alloc failure unit test with vector_push wrapper
cc -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -c src/parser_core.c -o parser_core_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_init.c -o parser_init_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_decl.c -o parser_decl_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_flow.c -o parser_flow_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_toplevel.c -o parser_toplevel_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_expr.c -o parser_expr_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_stmt.c -o parser_stmt_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/parser_types.c -o parser_types_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/symtable_core.c -o symtable_core_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/symtable_globals.c -o symtable_globals_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/symtable_struct.c -o symtable_struct_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/ast_clone.c -o ast_clone_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/ast_expr.c -o ast_expr_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/ast_stmt.c -o ast_stmt_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/lexer.c -o lexer_alloc.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_alloc.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_alloc.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_alloc.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_parser_alloc_fail.c" -o "$DIR/test_parser_alloc_fail.o"
cc -o "$DIR/parser_alloc_tests" parser_core_fail.o parser_init_fail.o parser_decl_fail.o parser_flow_fail.o parser_toplevel_fail.o parser_expr_fail.o parser_stmt_fail.o parser_types_fail.o symtable_core_fail.o symtable_globals_fail.o symtable_struct_fail.o ast_clone_fail.o ast_expr_fail.o ast_stmt_fail.o lexer_alloc.o vector_alloc.o util_alloc.o error_alloc.o "$DIR/test_parser_alloc_fail.o"
rm -f parser_core_fail.o parser_init_fail.o parser_decl_fail.o parser_flow_fail.o parser_toplevel_fail.o parser_expr_fail.o parser_stmt_fail.o parser_types_fail.o symtable_core_fail.o symtable_globals_fail.o symtable_struct_fail.o ast_clone_fail.o ast_expr_fail.o ast_stmt_fail.o lexer_alloc.o vector_alloc.o util_alloc.o error_alloc.o "$DIR/test_parser_alloc_fail.o"
# build ir_core unit test binary with malloc wrapper
cc -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -c src/ir_core.c -o ir_core_test.o
cc -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -c src/util.c -o util_ircore.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_ircore.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_ircore.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_ir_core.c" -o "$DIR/test_ir_core.o"
cc -o "$DIR/ir_core_tests" ir_core_test.o util_ircore.o error_ircore.o label_ircore.o "$DIR/test_ir_core.o"
rm -f ir_core_test.o util_ircore.o error_ircore.o label_ircore.o "$DIR/test_ir_core.o"
# build conditional expression regression test
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/cond_expr_tests" "$DIR/unit/test_cond_expr.c" \
    src/semantic_expr.c src/semantic_arith.c src/semantic_mem.c \
    src/semantic_call.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/ast_expr.c src/vector.c src/util.c src/ir_core.c \
    src/error.c src/label.c
# build complex expression semantic tests
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/complex_expr_tests" "$DIR/unit/test_complex_expr.c" \
    src/semantic_expr.c src/semantic_arith.c src/semantic_mem.c \
    src/semantic_call.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/ast_expr.c src/vector.c src/util.c src/ir_core.c \
    src/error.c src/label.c
# build sizeof pointer evaluation test
# eval sizeof with small helper modules
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/eval_sizeof_tests" "$DIR/unit/test_eval_sizeof.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/eval_offsetof_tests" "$DIR/unit/test_eval_offsetof.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
# build numeric constant overflow regression test
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/number_overflow" "$DIR/unit/test_number_overflow.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
# build numeric literal suffix tests
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/number_suffix" "$DIR/unit/test_number_suffix.c" \
    src/ast_expr.c src/semantic_expr.c src/semantic_arith.c \
    src/semantic_mem.c src/semantic_call.c src/consteval.c \
    src/symtable_core.c src/symtable_struct.c src/vector.c src/util.c src/ir_core.c \
    src/error.c src/label.c
# build constant arithmetic overflow regression test
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/consteval_overflow" "$DIR/unit/test_consteval_overflow.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
# build strbuf overflow regression test
cc -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_overflow_impl.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_strbuf.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_strbuf_overflow.c" -o "$DIR/test_strbuf_overflow.o"
cc -o "$DIR/strbuf_overflow" strbuf_overflow_impl.o util_strbuf.o "$DIR/test_strbuf_overflow.o"
rm -f strbuf_overflow_impl.o util_strbuf.o "$DIR/test_strbuf_overflow.o"
# build collect_funcs overflow regression test
cc -Iinclude -Wall -Wextra -std=c99 "$DIR/unit/test_collect_funcs_overflow.c" -o "$DIR/collect_funcs_overflow"
# build waitpid EINTR regression test
cc -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_eintr_impl.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_eintr.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_waitpid_retry.c" -o "$DIR/test_waitpid_retry.o"
cc -o "$DIR/waitpid_retry" strbuf_eintr_impl.o util_eintr.o "$DIR/test_waitpid_retry.o"
rm -f strbuf_eintr_impl.o util_eintr.o "$DIR/test_waitpid_retry.o"
# build invalid macro parse test
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_invalid_macro.c" -o "$DIR/test_invalid_macro.o"
cc -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_invalid.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_invalid.o
cc -o "$DIR/invalid_macro_tests" "$DIR/test_invalid_macro.o" vector_invalid.o util_invalid.o
rm -f "$DIR/test_invalid_macro.o" vector_invalid.o util_invalid.o
# build preprocessor alloc failure tests
cc -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -c "$DIR/unit/test_preproc_alloc_fail.c" -o "$DIR/test_preproc_alloc_fail.o"
cc -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_preproc.o
cc -o "$DIR/preproc_alloc_tests" "$DIR/test_preproc_alloc_fail.o" vector_preproc.o
rm -f "$DIR/test_preproc_alloc_fail.o" vector_preproc.o
# build add_macro push failure test
cc -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -c "$DIR/unit/test_add_macro_fail.c" -o "$DIR/test_add_macro_fail.o"
cc -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_addmacro.o
cc -o "$DIR/add_macro_fail_tests" "$DIR/test_add_macro_fail.o" vector_addmacro.o
rm -f "$DIR/test_add_macro_fail.o" vector_addmacro.o
# build variadic macro tests
cc -Iinclude -Wall -Wextra -std=c99 -c src/preproc_macros.c -o preproc_variadic.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_variadic.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_variadic.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_variadic.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_variadic_macro.c" -o "$DIR/test_variadic_macro.o"
cc -o "$DIR/variadic_macro_tests" preproc_variadic.o strbuf_variadic.o vector_variadic.o util_variadic.o "$DIR/test_variadic_macro.o"
rm -f preproc_variadic.o strbuf_variadic.o vector_variadic.o util_variadic.o "$DIR/test_variadic_macro.o"
# build pack pragma layout tests
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/pack_pragma_tests" "$DIR/unit/test_pack_pragma.c"
# build create_temp_file path length regression test
cc -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -ffunction-sections -fdata-sections -c src/compile.c -o compile_temp.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_temp_file.c" -o "$DIR/test_temp_file.o"
cc -Wl,--gc-sections -o "$DIR/temp_file_tests" compile_temp.o "$DIR/test_temp_file.o"
rm -f compile_temp.o "$DIR/test_temp_file.o"
# build compile_source_obj temp file failure test
cc -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -Dcompile_unit=test_compile_unit -ffunction-sections -fdata-sections -c src/compile.c -o compile_obj_fail.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_compile_obj_fail.c" -o "$DIR/test_compile_obj_fail.o"
cc -Wl,--gc-sections -o "$DIR/compile_obj_fail" compile_obj_fail.o "$DIR/test_compile_obj_fail.o"
rm -f compile_obj_fail.o "$DIR/test_compile_obj_fail.o"
# build constant folding tests
cc -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Dfree=test_free -c src/ir_core.c -o ir_fold.o
cc -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Dfree=test_free -c src/util.c -o util_fold.o
cc -Iinclude -Wall -Wextra -std=c99 -Dcalloc=test_calloc -Dfree=test_free -c src/opt_fold.c -o opt_fold_main.o
cc -Iinclude -Wall -Wextra -std=c99 -Dfree=test_free -c src/label.c -o label_fold.o
cc -Iinclude -Wall -Wextra -std=c99 -Dfree=test_free -c src/error.c -o error_fold.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_opt_fold.c" -o "$DIR/test_opt_fold.o"
cc -o "$DIR/opt_fold_tests" ir_fold.o util_fold.o opt_fold_main.o label_fold.o error_fold.o "$DIR/test_opt_fold.o"
rm -f ir_fold.o util_fold.o opt_fold_main.o label_fold.o error_fold.o "$DIR/test_opt_fold.o"
# build unreachable block elimination test
cc -Iinclude -Wall -Wextra -std=c99 -c src/ir_core.c -o ir_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt.c -o opt_main.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_constprop.c -o opt_constprop_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_cse.c -o opt_cse_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_fold.c -o opt_fold_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_dce.c -o opt_dce_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_inline.c -o opt_inline_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_unreachable.c -o opt_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_alias.c -o opt_alias_unreach.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_opt_unreachable.c" -o "$DIR/test_opt_unreachable.o"
cc -o "$DIR/opt_unreachable_tests" ir_unreach.o util_unreach.o label_unreach.o error_unreach.o \
    opt_main.o opt_constprop_unreach.o opt_cse_unreach.o opt_fold_unreach.o \
    opt_dce_unreach.o opt_inline_unreach.o opt_unreach.o opt_alias_unreach.o "$DIR/test_opt_unreachable.o"
rm -f ir_unreach.o util_unreach.o label_unreach.o error_unreach.o opt_main.o \
      opt_constprop_unreach.o opt_cse_unreach.o opt_fold_unreach.o \
      opt_dce_unreach.o opt_inline_unreach.o opt_unreach.o opt_alias_unreach.o "$DIR/test_opt_unreachable.o"
cc -Iinclude -Wall -Wextra -std=c99 -c src/ir_core.c -o ir_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt.c -o opt_main_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_constprop.c -o opt_constprop_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_cse.c -o opt_cse_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_fold.c -o opt_fold_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_licm.c -o opt_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_inline.c -o opt_inline_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_unreachable.c -o opt_unreach_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_dce.c -o opt_dce_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/opt_alias.c -o opt_alias_licm.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_opt_licm.c" -o "$DIR/test_opt_licm.o"
cc -o "$DIR/opt_licm_tests" ir_licm.o util_licm.o label_licm.o error_licm.o \
    opt_main_licm.o opt_constprop_licm.o opt_cse_licm.o opt_fold_licm.o opt_licm.o \
    opt_inline_licm.o opt_unreach_licm.o opt_dce_licm.o opt_alias_licm.o "$DIR/test_opt_licm.o"
rm -f ir_licm.o util_licm.o label_licm.o error_licm.o opt_main_licm.o \
      opt_constprop_licm.o opt_cse_licm.o opt_fold_licm.o opt_licm.o \
      opt_inline_licm.o opt_unreach_licm.o opt_dce_licm.o opt_alias_licm.o "$DIR/test_opt_licm.o"
# run unit tests
"$DIR/unit_tests"
"$DIR/cli_tests"
"$DIR/parser_alloc_tests"
"$DIR/ir_core_tests"
"$DIR/opt_fold_tests"
"$DIR/opt_unreachable_tests"
"$DIR/opt_licm_tests"
# remaining unit test binaries
"$DIR/cond_expr_tests"
"$DIR/complex_expr_tests"
"$DIR/eval_sizeof_tests"
"$DIR/eval_offsetof_tests"
"$DIR/number_overflow"
"$DIR/number_suffix"
"$DIR/waitpid_retry"
"$DIR/temp_file_tests"
"$DIR/compile_obj_fail"
"$DIR/preproc_alloc_tests"
"$DIR/add_macro_fail_tests"
"$DIR/variadic_macro_tests"
"$DIR/pack_pragma_tests"
"$DIR/invalid_macro_tests"
# separator for clarity
echo "======="
# regression test for strbuf overflow handling
err=$(mktemp)
set +e
"$DIR/strbuf_overflow" 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "string buffer too large" "$err"; then
    echo "Test strbuf_overflow failed"
    fail=1
fi
rm -f "$err" "$DIR/strbuf_overflow"
# regression test for constant expression overflow handling
err=$(mktemp)
set +e
"$DIR/consteval_overflow" 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "Constant overflow" "$err"; then
    echo "Test consteval_overflow failed"
    fail=1
fi
rm -f "$err" "$DIR/consteval_overflow"
# regression test for collect_funcs overflow handling
err=$(mktemp)
set +e
"$DIR/collect_funcs_overflow" 2> "$err" >/dev/null
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "too many inline functions" "$err"; then
    echo "Test collect_funcs_overflow failed"
    fail=1
fi
rm -f "$err" "$DIR/collect_funcs_overflow"
# run integration tests
"$DIR/run_tests.sh"
exit $fail

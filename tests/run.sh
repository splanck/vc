#!/bin/sh
# exit on unhandled errors
set -e
DIR=$(dirname "$0")
BINARY="$DIR/../vc"
# default C compiler
CC=${CC:-cc}
# track failing regression tests
fail=0

. "$DIR/helpers.sh"
ensure_compiler
ensure_libc
# build unit test binary
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -DNO_VECTOR_FREE_STUB -c src/util.c -o util_unit.o
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/unit_tests" "$DIR/unit/test_lexer_parser.c" \
    src/parser_core.c src/parser_init.c src/parser_decl_var.c \
    src/parser_decl_struct.c src/parser_decl_enum.c \
    src/parser_flow.c src/parser_toplevel.c src/parser_expr.c \
    src/parser_expr_primary.c src/parser_expr_binary.c \
    src/parser_stmt.c src/parser_types.c src/symtable_core.c \
    src/symtable_globals.c src/symtable_struct.c src/ast_clone.c \
    src/ast_expr.c src/ast_stmt_create.c src/ast_stmt_free.c src/lexer.c util_unit.o \
    src/vector.c src/error.c src/token_names.c src/parser_toplevel_func.c \
    src/parser_toplevel_var.c src/parser_expr_ops.c src/parser_expr_literal.c \
    src/lexer_ident.c src/lexer_scan_numeric.c src/ast_expr_binary.c \
    src/ast_expr_control.c src/ast_expr_literal.c src/ast_expr_type.c
rm -f util_unit.o
# build cli unit test binary with vector_push wrapper
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/cli.c -o cli_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/cli_env.c -o cli_env_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/cli_opts.c -o cli_opts_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -DPROJECT_ROOT=\"$(pwd)\" -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/preproc_path.c -o preproc_path_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/include_path_cache.c -o include_path_cache_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c "$DIR/unit/test_cli.c" -o "$DIR/test_cli.o"
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/vector.c -o vector_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -DNO_VECTOR_FREE_STUB -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free -c src/util.c -o util_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dpreproc_set_internal_libc_dir=preproc_set_internal_libc_dir_stub -c "$DIR/unit/test_cli_stubs.c" -o "$DIR/test_cli_stubs.o"
$CC -o "$DIR/cli_tests" cli_test.o cli_env_test.o cli_opts_test.o preproc_path_test.o include_path_cache_test.o "$DIR/test_cli.o" vector_test.o util_test.o "$DIR/test_cli_stubs.o"
rm -f cli_test.o cli_env_test.o cli_opts_test.o preproc_path_test.o include_path_cache_test.o "$DIR/test_cli.o" vector_test.o util_test.o "$DIR/test_cli_stubs.o"
# build parser alloc failure unit test with vector_push wrapper
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -c src/parser_core.c -o parser_core_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_init.c -o parser_init_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_decl_var.c -o parser_decl_var_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_decl_struct.c -o parser_decl_struct_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_decl_enum.c -o parser_decl_enum_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_flow.c -o parser_flow_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_toplevel.c -o parser_toplevel_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_expr.c -o parser_expr_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_expr_primary.c -o parser_expr_primary_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_expr_binary.c -o parser_expr_binary_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_stmt.c -o parser_stmt_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_types.c -o parser_types_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_toplevel_func.c -o parser_toplevel_func_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_toplevel_var.c -o parser_toplevel_var_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_expr_ops.c -o parser_expr_ops_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/parser_expr_literal.c -o parser_expr_literal_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/lexer_ident.c -o lexer_ident_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/lexer_scan_numeric.c -o lexer_scan_numeric_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_expr_binary.c -o ast_expr_binary_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_expr_control.c -o ast_expr_control_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_expr_literal.c -o ast_expr_literal_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_expr_type.c -o ast_expr_type_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/token_names.c -o token_names_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_table.c -o preproc_table_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/symtable_core.c -o symtable_core_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/symtable_globals.c -o symtable_globals_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/symtable_struct.c -o symtable_struct_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_clone.c -o ast_clone_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_expr.c -o ast_expr_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_stmt_create.c -o ast_stmt_create_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ast_stmt_free.c -o ast_stmt_free_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -c src/lexer.c -o lexer_alloc.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_alloc.o
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -c src/util.c -o util_alloc.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_alloc.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_parser_alloc_fail.c" -o "$DIR/test_parser_alloc_fail.o"
$CC -o "$DIR/parser_alloc_tests" parser_core_fail.o parser_init_fail.o parser_decl_var_fail.o parser_decl_struct_fail.o parser_decl_enum_fail.o parser_flow_fail.o parser_toplevel_fail.o parser_expr_fail.o parser_expr_primary_fail.o parser_expr_binary_fail.o parser_stmt_fail.o parser_types_fail.o parser_toplevel_func_fail.o parser_toplevel_var_fail.o parser_expr_ops_fail.o parser_expr_literal_fail.o lexer_ident_fail.o lexer_scan_numeric_fail.o ast_expr_binary_fail.o ast_expr_control_fail.o ast_expr_literal_fail.o ast_expr_type_fail.o token_names_fail.o preproc_table_fail.o symtable_core_fail.o symtable_globals_fail.o symtable_struct_fail.o ast_clone_fail.o ast_expr_fail.o ast_stmt_create_fail.o ast_stmt_free_fail.o lexer_alloc.o vector_alloc.o util_alloc.o error_alloc.o "$DIR/test_parser_alloc_fail.o"
rm -f parser_core_fail.o parser_init_fail.o parser_decl_var_fail.o parser_decl_struct_fail.o parser_decl_enum_fail.o parser_flow_fail.o parser_toplevel_fail.o parser_expr_fail.o parser_expr_primary_fail.o parser_expr_binary_fail.o parser_stmt_fail.o parser_types_fail.o parser_toplevel_func_fail.o parser_toplevel_var_fail.o parser_expr_ops_fail.o parser_expr_literal_fail.o lexer_ident_fail.o lexer_scan_numeric_fail.o ast_expr_binary_fail.o ast_expr_control_fail.o ast_expr_literal_fail.o ast_expr_type_fail.o token_names_fail.o preproc_table_fail.o symtable_core_fail.o symtable_globals_fail.o symtable_struct_fail.o ast_clone_fail.o ast_expr_fail.o ast_stmt_create_fail.o ast_stmt_free_fail.o lexer_alloc.o vector_alloc.o util_alloc.o error_alloc.o "$DIR/test_parser_alloc_fail.o"
# build ir_core unit test binary with malloc wrapper
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -c src/ir_core.c -o ir_core_test.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -c src/util.c -o util_ircore.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_ircore.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_ircore.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_ir_core.c" -o "$DIR/test_ir_core.o"
$CC -o "$DIR/ir_core_tests" ir_core_test.o util_ircore.o error_ircore.o label_ircore.o "$DIR/test_ir_core.o"
rm -f ir_core_test.o util_ircore.o error_ircore.o label_ircore.o "$DIR/test_ir_core.o"
# build conditional expression regression test
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/cond_expr_tests" "$DIR/unit/test_cond_expr.c" \
    src/semantic_expr.c src/semantic_arith.c src/semantic_mem.c \
    src/semantic_call.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/ast_expr.c src/vector.c src/util.c src/ir_core.c \
    src/error.c src/label.c
# build complex expression semantic tests
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/complex_expr_tests" "$DIR/unit/test_complex_expr.c" \
    src/semantic_expr.c src/semantic_arith.c src/semantic_mem.c \
    src/semantic_call.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/ast_expr.c src/vector.c src/util.c src/ir_core.c \
    src/error.c src/label.c
# build sizeof pointer evaluation test
# eval sizeof with small helper modules
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/eval_sizeof_tests" "$DIR/unit/test_eval_sizeof.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/eval_offsetof_tests" "$DIR/unit/test_eval_offsetof.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
# build numeric constant overflow regression test
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/number_overflow" "$DIR/unit/test_number_overflow.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
# build numeric literal suffix tests
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/number_suffix" "$DIR/unit/test_number_suffix.c" \
    src/ast_expr.c src/semantic_expr.c src/semantic_arith.c \
    src/semantic_mem.c src/semantic_call.c src/consteval.c \
    src/symtable_core.c src/symtable_struct.c src/vector.c src/util.c src/ir_core.c \
    src/error.c src/label.c
# build constant arithmetic overflow regression test
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/consteval_overflow" "$DIR/unit/test_consteval_overflow.c" \
    src/ast_expr.c src/consteval.c src/symtable_core.c src/symtable_struct.c \
    src/util.c src/error.c
# build strbuf overflow regression test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_overflow_impl.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_strbuf.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_strbuf_overflow.c" -o "$DIR/test_strbuf_overflow.o"
$CC -o "$DIR/strbuf_overflow" strbuf_overflow_impl.o util_strbuf.o "$DIR/test_strbuf_overflow.o"
rm -f strbuf_overflow_impl.o util_strbuf.o "$DIR/test_strbuf_overflow.o"
# build text line append failure test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_textfail_impl.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_textfail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dstrbuf_append=test_strbuf_append \
    -c "$DIR/unit/test_text_line_fail.c" -o "$DIR/test_text_line_fail.o"
$CC -o "$DIR/text_line_fail" strbuf_textfail_impl.o util_textfail.o "$DIR/test_text_line_fail.o"
rm -f strbuf_textfail_impl.o util_textfail.o "$DIR/test_text_line_fail.o"
# build command_to_string failure test
$CC -Iinclude -Wall -Wextra -std=c99 \
    -Dstrbuf_init=test_strbuf_init -Dstrbuf_append=test_strbuf_append \
    -Dstrbuf_free=test_strbuf_free -Dposix_spawnp=test_posix_spawnp \
    -Dmalloc=test_malloc -Drealloc=test_realloc \
    -c src/command.c -o command_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_command_fail.c" -o "$DIR/test_command_fail.o"
$CC -o "$DIR/command_fail" command_fail.o "$DIR/test_command_fail.o"
rm -f command_fail.o "$DIR/test_command_fail.o"
# build vc_strtoul_unsigned negative value rejection test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_strtoul_unsigned.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_vc_strtoul_unsigned.c" -o "$DIR/test_vc_strtoul_unsigned.o"
$CC -o "$DIR/vc_strtoul_unsigned" util_strtoul_unsigned.o "$DIR/test_vc_strtoul_unsigned.o"
rm -f util_strtoul_unsigned.o "$DIR/test_vc_strtoul_unsigned.o"
# build vc_strtoul_size negative value rejection test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_strtoul_size.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_vc_strtoul_size.c" -o "$DIR/test_vc_strtoul_size.o"
$CC -o "$DIR/vc_strtoul_size" util_strtoul_size.o "$DIR/test_vc_strtoul_size.o"
rm -f util_strtoul_size.o "$DIR/test_vc_strtoul_size.o"
# build builtin counter wraparound regression test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_builtin.c -o preproc_builtin_wrap.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_wrap.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_wrap.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_preproc_counter_wrap.c" -o "$DIR/test_preproc_counter_wrap.o"
$CC -o "$DIR/preproc_counter_wrap" preproc_builtin_wrap.o strbuf_wrap.o util_wrap.o "$DIR/test_preproc_counter_wrap.o"
rm -f preproc_builtin_wrap.o strbuf_wrap.o util_wrap.o "$DIR/test_preproc_counter_wrap.o"
# build collect_funcs overflow regression test
$CC -Iinclude -Wall -Wextra -std=c99 "$DIR/unit/test_collect_funcs_overflow.c" -o "$DIR/collect_funcs_overflow"
# build parse_inline_hint descriptor close test
$CC -Iinclude -Wall -Wextra -std=c99 -o "$DIR/parse_inline_hint_tests" "$DIR/unit/test_parse_inline_hint.c"
# build waitpid EINTR regression test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_eintr_impl.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_eintr.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_waitpid_retry.c" -o "$DIR/test_waitpid_retry.o"
$CC -o "$DIR/waitpid_retry" strbuf_eintr_impl.o util_eintr.o "$DIR/test_waitpid_retry.o"
rm -f strbuf_eintr_impl.o util_eintr.o "$DIR/test_waitpid_retry.o"
# build append_env_paths tests
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/append_env_paths_colon" "$DIR/unit/test_append_env_paths_colon.c" \
    src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
$CC -Iinclude -Wall -Wextra -std=c99 -D_WIN32 \
    -o "$DIR/append_env_paths_semicolon" "$DIR/unit/test_append_env_paths_semicolon.c" \
    src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
# build append_env_paths failure test
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/preproc_path.c -o preproc_path_append_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/include_path_cache.c -o include_path_cache_append_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/vector.c -o vector_append_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -DNO_VECTOR_FREE_STUB \
    -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/util.c -o util_append_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c "$DIR/unit/test_append_env_paths_fail.c" -o "$DIR/test_append_env_paths_fail.o"
$CC -o "$DIR/append_env_paths_fail" preproc_path_append_fail.o include_path_cache_append_fail.o \
    vector_append_fail.o util_append_fail.o "$DIR/test_append_env_paths_fail.o"
rm -f preproc_path_append_fail.o include_path_cache_append_fail.o vector_append_fail.o util_append_fail.o \
      "$DIR/test_append_env_paths_fail.o"
# build invalid macro parse test
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_invalid_macro.c" -o "$DIR/test_invalid_macro.o"
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_invalid.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_invalid.o
$CC -o "$DIR/invalid_macro_tests" "$DIR/test_invalid_macro.o" vector_invalid.o util_invalid.o
rm -f "$DIR/test_invalid_macro.o" vector_invalid.o util_invalid.o
# build preprocessor alloc failure tests
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -c "$DIR/unit/test_preproc_alloc_fail.c" -o "$DIR/test_preproc_alloc_fail.o"
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_preproc.o
$CC -o "$DIR/preproc_alloc_tests" "$DIR/test_preproc_alloc_fail.o" vector_preproc.o
rm -f "$DIR/test_preproc_alloc_fail.o" vector_preproc.o
# build add_macro push failure test
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push -c "$DIR/unit/test_add_macro_fail.c" -o "$DIR/test_add_macro_fail.o"
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_addmacro.o
$CC -o "$DIR/add_macro_fail_tests" "$DIR/test_add_macro_fail.o" vector_addmacro.o
rm -f "$DIR/test_add_macro_fail.o" vector_addmacro.o
# build collect_include_dirs failure test
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/preproc_path.c -o preproc_path_collect_dest_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/include_path_cache.c -o include_path_cache_collect_dest_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/vector.c -o vector_collect_dest_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -DNO_VECTOR_FREE_STUB \
    -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c src/util.c -o util_collect_dest_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dvector_push=test_vector_push \
    -Dmalloc=test_malloc -Dcalloc=test_calloc -Drealloc=test_realloc -Dfree=test_free \
    -c "$DIR/unit/test_collect_include_dest_fail.c" -o "$DIR/test_collect_include_dest_fail.o"
$CC -o "$DIR/collect_include_dest_fail" preproc_path_collect_dest_fail.o include_path_cache_collect_dest_fail.o \
    vector_collect_dest_fail.o util_collect_dest_fail.o "$DIR/test_collect_include_dest_fail.o"
rm -f preproc_path_collect_dest_fail.o include_path_cache_collect_dest_fail.o vector_collect_dest_fail.o util_collect_dest_fail.o \
      "$DIR/test_collect_include_dest_fail.o"
# build variadic macro tests
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_expand.c -o preproc_expand.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_table.c -o preproc_table.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_variadic.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_variadic.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_variadic.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_variadic_macro.c" -o "$DIR/test_variadic_macro.o"
$CC -o "$DIR/variadic_macro_tests" preproc_expand.o preproc_table.o strbuf_variadic.o vector_variadic.o util_variadic.o "$DIR/test_variadic_macro.o"
rm -f preproc_expand.o preproc_table.o strbuf_variadic.o vector_variadic.o util_variadic.o "$DIR/test_variadic_macro.o"
# build macro stringize escape test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_expand.c -o preproc_expand.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_table.c -o preproc_table.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_stringize.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_stringize.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_stringize.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_macro_stringize_escape.c" -o "$DIR/test_macro_stringize_escape.o"
$CC -o "$DIR/macro_stringize_escape" preproc_expand.o preproc_table.o strbuf_stringize.o vector_stringize.o util_stringize.o "$DIR/test_macro_stringize_escape.o"
rm -f preproc_expand.o preproc_table.o strbuf_stringize.o vector_stringize.o util_stringize.o "$DIR/test_macro_stringize_escape.o"
# build literal argument parsing tests
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_expand.c -o preproc_expand.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_table.c -o preproc_table.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_litargs.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_litargs.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_litargs.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/preproc_builtin.c -o preproc_builtin_litargs.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_preproc_literal_args.c" -o "$DIR/test_preproc_literal_args.o"
$CC -o "$DIR/preproc_literal_args" preproc_expand.o preproc_table.o strbuf_litargs.o vector_litargs.o util_litargs.o preproc_builtin_litargs.o "$DIR/test_preproc_literal_args.o"
rm -f preproc_expand.o preproc_table.o strbuf_litargs.o vector_litargs.o util_litargs.o preproc_builtin_litargs.o "$DIR/test_preproc_literal_args.o"
# build pack pragma layout tests
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/pack_pragma_tests" "$DIR/unit/test_pack_pragma.c"
# build read_file_lines large input test
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/read_file_lines_large" "$DIR/unit/test_read_file_lines_large.c" \
    src/preproc_file_io.c src/util.c
# build preprocessing of stdio.h regression test
MULTIARCH=$(gcc -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
GCC_INCLUDE_DIR=$(gcc -print-file-name=include)
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/preproc_stdio" "$DIR/unit/test_preproc_stdio.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/preproc_stdio_skip" "$DIR/unit/test_preproc_stdio_skip.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_multi_stdheaders" "$DIR/unit/test_preproc_multi_stdheaders.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_has_include" "$DIR/unit/test_preproc_has_include.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_has_include_macro" "$DIR/unit/test_preproc_has_include_macro.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_has_include_alloc_fail" "$DIR/unit/test_preproc_has_include_alloc_fail.c"
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_include_comment" "$DIR/unit/test_preproc_include_comment.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_ifmacro" "$DIR/unit/test_preproc_ifmacro.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_line" "$DIR/unit/test_preproc_line.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_line_macro" "$DIR/unit/test_preproc_line_macro.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_line_decrease" "$DIR/unit/test_preproc_line_decrease.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/gcc_line_marker" "$DIR/unit/test_gcc_line_marker.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_hash_noop" "$DIR/unit/test_preproc_hash_noop.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_crlf" "$DIR/unit/test_preproc_crlf.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_pack_macro" "$DIR/unit/test_preproc_pack_macro.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_pack_push" "$DIR/unit/test_preproc_pack_push.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_pack_invalid" "$DIR/unit/test_preproc_pack_invalid.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_pragma_macro" "$DIR/unit/test_preproc_pragma_macro.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_defined_macro" "$DIR/unit/test_preproc_defined_macro.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_unterm_comment" "$DIR/unit/test_preproc_unterm_comment.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/preproc_pragma" "$DIR/unit/test_preproc_pragma.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_pragma_unknown" "$DIR/unit/test_preproc_pragma_unknown.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/preproc_builtin_extra" "$DIR/unit/test_builtin_macros.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_charlit" "$DIR/unit/test_preproc_charlit.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_token_paste" "$DIR/unit/test_preproc_token_paste.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/preproc_counter_base" "$DIR/unit/test_predef_counter_base.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_independent" "$DIR/unit/test_preproc_independent.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_counter_reset" "$DIR/unit/test_preproc_counter_reset.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_errwarn" "$DIR/unit/test_preproc_errwarn.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_expand_size" "$DIR/unit/test_preproc_expand_size.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/preproc_system_header" "$DIR/unit/test_preproc_system_header.c" \
    src/preproc_file.c src/preproc_directives.c src/preproc_file_io.c \
    src/preproc_expand.c src/preproc_table.c src/preproc_builtin.c \
    src/preproc_args.c src/preproc_cond.c src/preproc_expr.c \
    src/preproc_include.c src/preproc_includes.c src/include_path_cache.c src/preproc_path.c \
    src/vector.c src/strbuf.c src/util.c src/error.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/collect_include_sysroot" "$DIR/unit/test_collect_include_sysroot.c" \
    src/include_path_cache.c src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/internal_libc_sysroot" "$DIR/unit/test_internal_libc_sysroot.c" \
    src/include_path_cache.c src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
$CC -Iinclude -Wall -Wextra -std=c99 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/vc_sysinclude" "$DIR/unit/test_vc_sysinclude.c" \
    src/include_path_cache.c src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
$CC -Iinclude -Wall -Wextra -std=c99 -D_WIN32 \
    -DMULTIARCH="${MULTIARCH}" -DGCC_INCLUDE_DIR="${GCC_INCLUDE_DIR}" \
    -o "$DIR/vc_sysinclude_win" "$DIR/unit/test_vc_sysinclude.c" \
    src/include_path_cache.c src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
$CC -Iinclude -Wall -Wextra -std=c99 -Dpopen=test_popen -DUNIT_TESTING -DNO_VECTOR_FREE_STUB \
    -o "$DIR/preproc_popen_fail" "$DIR/unit/test_preproc_popen_fail.c" \
    src/include_path_cache.c src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
$CC -Iinclude -Wall -Wextra -std=c99 -Dpopen=test_popen -DUNIT_TESTING -DNO_VECTOR_FREE_STUB \
    -o "$DIR/preproc_sysheaders_fail" "$DIR/unit/test_preproc_sysheaders_fail.c" \
    src/include_path_cache.c src/include_path_cache.c src/preproc_path.c src/vector.c src/util.c
# build std include dirs allocation failure test
$CC -Iinclude -Wall -Wextra -std=c99 -Dvc_alloc_or_exit=test_alloc_or_exit -c src/include_path_cache.c -o include_path_cache_fail_impl.o
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -c src/util.c -o util_std_dirs_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dvc_alloc_or_exit=test_alloc_or_exit \
    -c "$DIR/unit/test_std_dirs_alloc_fail.c" -o "$DIR/test_std_dirs_alloc_fail.o"
$CC -o "$DIR/std_dirs_alloc_fail" include_path_cache_fail_impl.o util_std_dirs_fail.o \
    "$DIR/test_std_dirs_alloc_fail.o"
rm -f include_path_cache_fail_impl.o util_std_dirs_fail.o "$DIR/test_std_dirs_alloc_fail.o"
# build create_temp_file path length regression test
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -ffunction-sections -fdata-sections -c src/compile.c -o compile_temp.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_temp_file.c" -o "$DIR/test_temp_file.o"
$CC -Wl,--gc-sections -o "$DIR/temp_file_tests" compile_temp.o "$DIR/test_temp_file.o"
rm -f compile_temp.o "$DIR/test_temp_file.o"
# build compile_source_obj temp file failure test
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -Dcompile_unit=test_compile_unit -ffunction-sections -fdata-sections -c src/compile.c -o compile_obj_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -Dcompile_unit=test_compile_unit -ffunction-sections -fdata-sections -c src/compile_link.c -o compile_link_obj_fail.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_compile_obj_fail.c" -o "$DIR/test_compile_obj_fail.o"
$CC -Wl,--gc-sections -o "$DIR/compile_obj_fail" compile_obj_fail.o compile_link_obj_fail.o "$DIR/test_compile_obj_fail.o"
rm -f compile_obj_fail.o compile_link_obj_fail.o "$DIR/test_compile_obj_fail.o"
# build vc_sysinclude long path test
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING \
    -Dcreate_startup_object=test_create_startup_object \
    -Dcommand_run=test_command_run -ffunction-sections -fdata-sections \
    -c src/compile_link.c -o compile_link_vc_sysinclude_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_vc_sysinclude_long.c" -o "$DIR/test_vc_sysinclude_long.o"
$CC -Wl,--gc-sections -o "$DIR/vc_sysinclude_long" compile_link_vc_sysinclude_long.o src/vector.c src/util.c src/error.c "$DIR/test_vc_sysinclude_long.o"
rm -f compile_link_vc_sysinclude_long.o "$DIR/test_vc_sysinclude_long.o"
# build overlong program path regression test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/cli.c -o cli_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/cli_env.c -o cli_env_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/cli_opts.c -o cli_opts_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -DPROJECT_ROOT="$(pwd)" -c src/preproc_path.c -o preproc_path_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/include_path_cache.c -o include_path_cache_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/vector.c -o vector_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_prog_path_long.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_prog_path_long.c" -o "$DIR/test_prog_path_long.o"
$CC -o "$DIR/prog_path_long" cli_prog_path_long.o cli_env_prog_path_long.o cli_opts_prog_path_long.o preproc_path_prog_path_long.o include_path_cache_prog_path_long.o vector_prog_path_long.o util_prog_path_long.o "$DIR/test_prog_path_long.o"
rm -f cli_prog_path_long.o cli_env_prog_path_long.o cli_opts_prog_path_long.o preproc_path_prog_path_long.o include_path_cache_prog_path_long.o vector_prog_path_long.o util_prog_path_long.o "$DIR/test_prog_path_long.o"
# build object/dependency name long filename test
$CC -Iinclude -Wall -Wextra -std=c99 -DUNIT_TESTING -ffunction-sections -fdata-sections -c src/compile_link.c -o compile_link_names.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_vc_names.c" -o "$DIR/test_vc_names.o"
$CC -Wl,--gc-sections -o "$DIR/vc_names_tests" compile_link_names.o "$DIR/test_vc_names.o"
rm -f compile_link_names.o "$DIR/test_vc_names.o"
# build constant folding tests
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Dfree=test_free -c src/ir_core.c -o ir_fold.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -Dcalloc=test_calloc -Dfree=test_free -c src/util.c -o util_fold.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dcalloc=test_calloc -Dfree=test_free -c src/opt_fold.c -o opt_fold_main.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dfree=test_free -c src/label.c -o label_fold.o
$CC -Iinclude -Wall -Wextra -std=c99 -Dfree=test_free -c src/error.c -o error_fold.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_opt_fold.c" -o "$DIR/test_opt_fold.o"
$CC -o "$DIR/opt_fold_tests" ir_fold.o util_fold.o opt_fold_main.o label_fold.o error_fold.o "$DIR/test_opt_fold.o"
rm -f ir_fold.o util_fold.o opt_fold_main.o label_fold.o error_fold.o "$DIR/test_opt_fold.o"
# build unreachable block elimination test
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ir_core.c -o ir_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt.c -o opt_main.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_constprop.c -o opt_constprop_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_cse.c -o opt_cse_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_fold.c -o opt_fold_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_dce.c -o opt_dce_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_inline.c -o opt_inline_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_unreachable.c -o opt_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_alias.c -o opt_alias_unreach.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_opt_unreachable.c" -o "$DIR/test_opt_unreachable.o"
$CC -o "$DIR/opt_unreachable_tests" ir_unreach.o util_unreach.o label_unreach.o error_unreach.o \
    opt_main.o opt_constprop_unreach.o opt_cse_unreach.o opt_fold_unreach.o \
    opt_dce_unreach.o opt_inline_unreach.o opt_unreach.o opt_alias_unreach.o "$DIR/test_opt_unreachable.o"
rm -f ir_unreach.o util_unreach.o label_unreach.o error_unreach.o opt_main.o \
      opt_constprop_unreach.o opt_cse_unreach.o opt_fold_unreach.o \
      opt_dce_unreach.o opt_inline_unreach.o opt_unreach.o opt_alias_unreach.o "$DIR/test_opt_unreachable.o"
$CC -Iinclude -Wall -Wextra -std=c99 -c src/ir_core.c -o ir_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/error.c -o error_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt.c -o opt_main_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_constprop.c -o opt_constprop_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_cse.c -o opt_cse_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_fold.c -o opt_fold_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_licm.c -o opt_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_inline.c -o opt_inline_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_unreachable.c -o opt_unreach_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_dce.c -o opt_dce_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c src/opt_alias.c -o opt_alias_licm.o
$CC -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_opt_licm.c" -o "$DIR/test_opt_licm.o"
$CC -o "$DIR/opt_licm_tests" ir_licm.o util_licm.o label_licm.o error_licm.o \
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
"$DIR/append_env_paths_colon"
"$DIR/append_env_paths_semicolon"
"$DIR/append_env_paths_fail"
"$DIR/parse_inline_hint_tests"
"$DIR/temp_file_tests"
"$DIR/compile_obj_fail"
"$DIR/vc_names_tests"
"$DIR/std_dirs_alloc_fail"
"$DIR/preproc_alloc_tests"
"$DIR/add_macro_fail_tests"
"$DIR/collect_include_dest_fail"
"$DIR/text_line_fail"
"$DIR/variadic_macro_tests"
"$DIR/macro_stringize_escape"
"$DIR/preproc_literal_args"
"$DIR/pack_pragma_tests"
"$DIR/read_file_lines_large"
"$DIR/preproc_stdio"
"$DIR/preproc_stdio_skip"
"$DIR/preproc_multi_stdheaders"
"$DIR/preproc_has_include"
"$DIR/preproc_has_include_macro"
"$DIR/preproc_has_include_alloc_fail"
"$DIR/preproc_include_comment"
"$DIR/preproc_ifmacro"
"$DIR/preproc_line"
"$DIR/preproc_line_macro"
"$DIR/preproc_line_decrease"
"$DIR/gcc_line_marker"
"$DIR/preproc_hash_noop"
"$DIR/preproc_crlf"
"$DIR/preproc_pack_macro"
"$DIR/preproc_pack_push"
"$DIR/preproc_pack_invalid"
"$DIR/preproc_pragma_macro"
"$DIR/preproc_defined_macro"
"$DIR/preproc_unterm_comment"
"$DIR/preproc_pragma"
"$DIR/preproc_pragma_unknown"
"$DIR/preproc_builtin_extra"
"$DIR/preproc_charlit"
"$DIR/preproc_token_paste"
"$DIR/preproc_counter_base"
"$DIR/preproc_counter_reset"
"$DIR/preproc_independent"
"$DIR/preproc_errwarn"
"$DIR/preproc_expand_size"
"$DIR/preproc_system_header"
"$DIR/collect_include_sysroot"
"$DIR/internal_libc_sysroot"
"$DIR/vc_sysinclude"
"$DIR/vc_sysinclude_win"
"$DIR/vc_sysinclude_long"
"$DIR/prog_path_long"
"$DIR/preproc_sysheaders_fail"
"$DIR/preproc_popen_fail"
"$DIR/invalid_macro_tests"
# separator for clarity
echo "======="
# regression test for strbuf overflow handling
err=$(safe_mktemp)
set +e
"$DIR/strbuf_overflow" 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "string buffer too large" "$err"; then
    echo "Test strbuf_overflow failed"
    fail=1
fi
rm -f "$err" "$DIR/strbuf_overflow"
# regression test for builtin counter overflow handling
err=$(safe_mktemp)
set +e
"$DIR/preproc_counter_wrap" 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "Builtin counter overflow" "$err"; then
    echo "Test preproc_counter_wrap failed"
    fail=1
fi
rm -f "$err" "$DIR/preproc_counter_wrap"
# regression test for constant expression overflow handling
err=$(safe_mktemp)
set +e
"$DIR/consteval_overflow" 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "Constant overflow" "$err"; then
    echo "Test consteval_overflow failed"
    fail=1
fi
rm -f "$err" "$DIR/consteval_overflow"
# regression test for command_to_string failure handling
err=$(safe_mktemp)
set +e
"$DIR/command_fail" 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ] || ! grep -q "posix_spawnp cmd arg1 arg2" "$err"; then
    echo "Test command_fail failed"
    fail=1
fi
rm -f "$err" "$DIR/command_fail"
# regression test for vc_strtoul_unsigned handling
err=$(safe_mktemp)
set +e
"$DIR/vc_strtoul_unsigned" > /dev/null 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ]; then
    echo "Test vc_strtoul_unsigned failed"
    fail=1
fi
rm -f "$err" "$DIR/vc_strtoul_unsigned"
# regression test for vc_strtoul_size handling
err=$(safe_mktemp)
set +e
"$DIR/vc_strtoul_size" > /dev/null 2> "$err"
ret=$?
set -e
if [ $ret -ne 0 ]; then
    echo "Test vc_strtoul_size failed"
    fail=1
fi
rm -f "$err" "$DIR/vc_strtoul_size"
# regression test for collect_funcs overflow handling
err=$(safe_mktemp)
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

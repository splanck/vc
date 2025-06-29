#!/bin/sh
set -e
DIR=$(dirname "$0")
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
# build ir_core unit test binary with malloc wrapper
cc -Iinclude -Wall -Wextra -std=c99 -Dmalloc=test_malloc -c src/ir_core.c -o ir_core_test.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_ircore.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/label.c -o label_ircore.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_ir_core.c" -o "$DIR/test_ir_core.o"
cc -o "$DIR/ir_core_tests" ir_core_test.o util_ircore.o label_ircore.o "$DIR/test_ir_core.o"
rm -f ir_core_test.o util_ircore.o label_ircore.o "$DIR/test_ir_core.o"
# build strbuf overflow regression test
cc -Iinclude -Wall -Wextra -std=c99 -c src/strbuf.c -o strbuf_overflow_impl.o
cc -Iinclude -Wall -Wextra -std=c99 -c src/util.c -o util_strbuf.o
cc -Iinclude -Wall -Wextra -std=c99 -c "$DIR/unit/test_strbuf_overflow.c" -o "$DIR/test_strbuf_overflow.o"
cc -o "$DIR/strbuf_overflow" strbuf_overflow_impl.o util_strbuf.o "$DIR/test_strbuf_overflow.o"
rm -f strbuf_overflow_impl.o util_strbuf.o "$DIR/test_strbuf_overflow.o"
# run unit tests
"$DIR/unit_tests"
"$DIR/cli_tests"
"$DIR/ir_core_tests"
# regression test for strbuf overflow handling
err=$(mktemp)
set +e
"$DIR/strbuf_overflow" 2> "$err"
ret=$?
set -e
if [ $ret -eq 0 ] || ! grep -q "string buffer too large" "$err"; then
    echo "Test strbuf_overflow failed"
    fail=1
fi
rm -f "$err" "$DIR/strbuf_overflow"
# run integration tests
"$DIR/run_tests.sh"

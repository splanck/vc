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
# run unit tests
"$DIR/unit_tests"
# run integration tests
"$DIR/run_tests.sh"

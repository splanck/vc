#!/bin/sh
set -e
DIR=$(dirname "$0")
# build the compiler
make
# build unit test binary
cc -Iinclude -Wall -Wextra -std=c99 \
    -o "$DIR/unit_tests" "$DIR/unit/test_lexer_parser.c" \
    src/lexer.c src/parser.c src/parser_expr.c src/parser_stmt.c \
    src/ast.c src/util.c src/vector.c
# run unit tests
"$DIR/unit_tests"
# run integration tests
"$DIR/run_tests.sh"

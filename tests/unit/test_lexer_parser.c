/*
 * Unit tests for the lexer and parser.
 *
 * Each routine below exercises a single feature of the front-end and
 * reports failures through a very small assertion helper.  The goal is
 * to keep these tests completely self contained so they can run in any
 * environment without additional infrastructure.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "ast.h"
#include "ast_expr.h"
#include "ast_stmt.h"
#include "vector.h"

/*
 * Number of failed assertions recorded so far.  The main function prints
 * a summary based on this counter.
 */
static int failures = 0;

/*
 * Basic assertion macro used by the tests.  When a condition fails the
 * message is printed along with file and line information and the global
 * failure count is incremented.
 */
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

/* Verify that a simple declaration is tokenised correctly. */
static void test_lexer_basic(void)
{
    const char *src = "int x;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(count >= 4);
    ASSERT(toks[0].type == TOK_KW_INT);
    ASSERT(toks[1].type == TOK_IDENT && strcmp(toks[1].lexeme, "x") == 0);
    ASSERT(toks[2].type == TOK_SEMI);
    ASSERT(toks[3].type == TOK_EOF);
    lexer_free_tokens(toks, count);
}

/* Ensure both line and block comments are skipped by the lexer. */
static void test_lexer_comments(void)
{
    const char *src =
        "int main() {\n"
        "    // line comment\n"
        "    /* block\n"
        "       comment */\n"
        "    return 0;\n"
        "}";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_KW_INT);
    ASSERT(toks[1].type == TOK_IDENT && strcmp(toks[1].lexeme, "main") == 0);
    ASSERT(toks[2].type == TOK_LPAREN);
    ASSERT(toks[3].type == TOK_RPAREN);
    ASSERT(toks[4].type == TOK_LBRACE);
    ASSERT(toks[5].type == TOK_KW_RETURN);
    ASSERT(toks[6].type == TOK_NUMBER && strcmp(toks[6].lexeme, "0") == 0);
    ASSERT(toks[7].type == TOK_SEMI);
    ASSERT(toks[8].type == TOK_RBRACE);
    ASSERT(toks[9].type == TOK_EOF);
    lexer_free_tokens(toks, count);
}

/* Tokenise the percent operator. */
static void test_lexer_percent(void)
{
    const char *src = "a % b;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[1].type == TOK_PERCENT);
    lexer_free_tokens(toks, count);
}

/* Lexer support for new type keywords such as short, long and bool. */
static void test_lexer_new_types(void)
{
    const char *src = "short s; long l; bool b; _Bool x; unsigned long long u;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_KW_SHORT);
    ASSERT(toks[3].type == TOK_KW_LONG);
    ASSERT(toks[6].type == TOK_KW_BOOL);
    ASSERT(toks[9].type == TOK_KW_BOOL);
    ASSERT(toks[12].type == TOK_KW_UNSIGNED);
    ASSERT(toks[13].type == TOK_KW_LONG && toks[14].type == TOK_KW_LONG);
    lexer_free_tokens(toks, count);
}

/* Lexing of the _Complex keyword. */
static void test_lexer_complex_kw(void)
{
    const char *src = "_Complex z;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_KW_COMPLEX);
    ASSERT(toks[1].type == TOK_IDENT && strcmp(toks[1].lexeme, "z") == 0);
    lexer_free_tokens(toks, count);
}

/* Lexing of an imaginary constant. */
static void test_lexer_imag_number(void)
{
    const char *src = "1.0i";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_IMAG_NUMBER);
    ASSERT(strcmp(toks[0].lexeme, "1.0i") == 0);
    lexer_free_tokens(toks, count);
}

/* Parsing of an imaginary constant as a complex literal. */
static void test_parser_imag_literal(void)
{
    const char *src = "2i";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_COMPLEX_LITERAL);
    ASSERT(expr->data.complex_lit.real == 0.0);
    ASSERT(expr->data.complex_lit.imag == 2.0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Parsing of a real plus imaginary constant as a complex literal. */
static void test_parser_complex_literal(void)
{
    const char *src = "1.0 + 2.0i";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_COMPLEX_LITERAL);
    ASSERT(expr->data.complex_lit.real == 1.0);
    ASSERT(expr->data.complex_lit.imag == 2.0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Parse a simple arithmetic expression and verify operator precedence. */
static void test_parser_expr(void)
{
    const char *src = "1 + 2 * 3";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr);
    ASSERT(expr->kind == EXPR_BINARY);
    ASSERT(expr->data.binary.op == BINOP_ADD);
    expr_t *left = expr->data.binary.left;
    expr_t *right = expr->data.binary.right;
    ASSERT(left && left->kind == EXPR_NUMBER && strcmp(left->data.number.value, "1") == 0);
    ASSERT(right && right->kind == EXPR_BINARY);
    ASSERT(right->data.binary.op == BINOP_MUL);
    ASSERT(right->data.binary.left->kind == EXPR_NUMBER && strcmp(right->data.binary.left->data.number.value, "2") == 0);
    ASSERT(right->data.binary.right->kind == EXPR_NUMBER && strcmp(right->data.binary.right->data.number.value, "3") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Parsing of a return statement with an expression. */
static void test_parser_stmt_return(void)
{
    const char *src = "return 5;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_RETURN);
    ASSERT(STMT_RET(stmt).expr->kind == EXPR_NUMBER &&
           strcmp(STMT_RET(stmt).expr->data.number.value, "5") == 0);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Parsing of a bare "return" statement. */
static void test_parser_stmt_return_void(void)
{
    const char *src = "return;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_RETURN);
    ASSERT(STMT_RET(stmt).expr == NULL);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Variable declaration including an initializer. */
static void test_parser_var_decl_init(void)
{
    const char *src = "int x = 5;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_VAR_DECL);
    ASSERT(strcmp(STMT_VAR_DECL(stmt).name, "x") == 0);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_INT);
    ASSERT(STMT_VAR_DECL(stmt).init && STMT_VAR_DECL(stmt).init->kind == EXPR_NUMBER &&
           strcmp(STMT_VAR_DECL(stmt).init->data.number.value, "5") == 0);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Declaration of a variable with the short type. */
static void test_parser_short_decl(void)
{
    const char *src = "short s;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_SHORT);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Declaration using the bool type. */
static void test_parser_bool_decl(void)
{
    const char *src = "bool b;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_BOOL);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Declaration using the _Bool type. */
static void test_parser__Bool_decl(void)
{
    const char *src = "_Bool b;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_BOOL);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Declaration of a float _Complex variable. */
static void test_parser_float_complex_decl(void)
{
    const char *src = "float _Complex z;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_FLOAT_COMPLEX);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Declaration of a double _Complex variable. */
static void test_parser_double_complex_decl(void)
{
    const char *src = "double _Complex z;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_DOUBLE_COMPLEX);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Declaration of a long double _Complex variable. */
static void test_parser_long_double_complex_decl(void)
{
    const char *src = "long double _Complex z;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_LDOUBLE_COMPLEX);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Lexing of the "static" storage class specifier. */
static void test_lexer_static_kw(void)
{
    const char *src = "static int x;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_KW_STATIC);
    lexer_free_tokens(toks, count);
}

/* Parsing of a static local variable declaration. */
static void test_parser_static_local(void)
{
    const char *src = "static int x;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt && stmt->kind == STMT_VAR_DECL && STMT_VAR_DECL(stmt).is_static);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Parse an array declaration with a constant size. */
static void test_parser_array_decl(void)
{
    const char *src = "int a[4];";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_VAR_DECL);
    ASSERT(strcmp(STMT_VAR_DECL(stmt).name, "a") == 0);
    ASSERT(STMT_VAR_DECL(stmt).type == TYPE_ARRAY);
    ASSERT(STMT_VAR_DECL(stmt).array_size == 4);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Parse a simple array indexing expression. */
static void test_parser_index_expr(void)
{
    const char *src = "a[1]";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_INDEX);
    ASSERT(expr->data.index.array->kind == EXPR_IDENT);
    ASSERT(strcmp(expr->data.index.array->data.ident.name, "a") == 0);
    ASSERT(expr->data.index.index->kind == EXPR_NUMBER &&
           strcmp(expr->data.index.index->data.number.value, "1") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Unary minus expression parsing. */
static void test_parser_unary_neg(void)
{
    const char *src = "-5";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr);
    ASSERT(expr->kind == EXPR_UNARY);
    ASSERT(expr->data.unary.op == UNOP_NEG);
    ASSERT(expr->data.unary.operand->kind == EXPR_NUMBER &&
           strcmp(expr->data.unary.operand->data.number.value, "5") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Pointer arithmetic should parse like integer addition. */
static void test_parser_pointer_arith(void)
{
    const char *src = "p + 1";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_BINARY);
    ASSERT(expr->data.binary.op == BINOP_ADD);
    ASSERT(expr->data.binary.left->kind == EXPR_IDENT &&
           strcmp(expr->data.binary.left->data.ident.name, "p") == 0);
    ASSERT(expr->data.binary.right->kind == EXPR_NUMBER &&
           strcmp(expr->data.binary.right->data.number.value, "1") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Modulo operator parsing. */
static void test_parser_mod(void)
{
    const char *src = "5 % 2";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_BINARY);
    ASSERT(expr->data.binary.op == BINOP_MOD);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Global variable initialization expression parsing. */
static void test_parser_global_init(void)
{
    const char *src = "int y = 1 + 2;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    symtable_t funcs; symtable_init(&funcs);
    func_t *fn = NULL; stmt_t *global = NULL;
    ASSERT(parser_parse_toplevel(&p, &funcs, &fn, &global));
    ASSERT(fn == NULL);
    ASSERT(global && global->kind == STMT_VAR_DECL);
    ASSERT(strcmp(STMT_VAR_DECL(global).name, "y") == 0);
    ASSERT(STMT_VAR_DECL(global).init && STMT_VAR_DECL(global).init->kind == EXPR_BINARY);
    symtable_free(&funcs);
    ast_free_stmt(global);
    lexer_free_tokens(toks, count);
}

/* Unary operator applied to a parenthesised expression. */
static void test_parser_unary_expr(void)
{
    const char *src = "-(1 + 2)";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_UNARY);
    ASSERT(expr->data.unary.op == UNOP_NEG);
    ASSERT(expr->data.unary.operand->kind == EXPR_BINARY);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Parse logical and/or expressions with precedence. */
static void test_parser_logical(void)
{
    const char *src = "1 && 2 || !0";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_BINARY);
    ASSERT(expr->data.binary.op == BINOP_LOGOR);
    ASSERT(expr->data.binary.left->kind == EXPR_BINARY &&
           expr->data.binary.left->data.binary.op == BINOP_LOGAND);
    ASSERT(expr->data.binary.right->kind == EXPR_UNARY &&
           expr->data.binary.right->data.unary.op == UNOP_NOT);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Conditional operator parsing (a ? b : c). */
static void test_parser_conditional(void)
{
    const char *src = "a ? b : c";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_COND);
    ASSERT(expr->data.cond.cond->kind == EXPR_IDENT);
    ASSERT(expr->data.cond.then_expr->kind == EXPR_IDENT);
    ASSERT(expr->data.cond.else_expr->kind == EXPR_IDENT);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Lexing of the sizeof keyword. */
static void test_lexer_sizeof(void)
{
    const char *src = "sizeof(int)";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_KW_SIZEOF);
    lexer_free_tokens(toks, count);
}

/* Parse a sizeof expression referring to a type. */
static void test_parser_sizeof(void)
{
    const char *src = "sizeof(int)";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_SIZEOF);
    ASSERT(expr->data.sizeof_expr.is_type);
    ASSERT(expr->data.sizeof_expr.type == TYPE_INT);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Parse a simple variadic call expression. */
static void test_parser_variadic_call(void)
{
    /* floating point literals are not yet recognized, so use integers */
    const char *src = "foo(1, 2)";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(count >= 6);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_CALL);
    ASSERT(expr->data.call.arg_count == 2);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

/* Parsing of a complete function definition. */
static void test_parser_func(void)
{
    const char *src = "int main() { return 0; }";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    func_t *fn = parser_parse_func(&p, NULL, 0, 0);
    ASSERT(fn);
    ASSERT(strcmp(fn->name, "main") == 0);
    ASSERT(fn->return_type == TYPE_INT);
    ASSERT(fn->body_count == 1);
    ASSERT(fn->body[0]->kind == STMT_RETURN);
    ast_free_func(fn);
    lexer_free_tokens(toks, count);
}

/* Nested block statement parsing. */
static void test_parser_block(void)
{
    const char *src = "{ int x; { int y; } }";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_BLOCK);
    ASSERT(STMT_BLOCK(stmt).count == 2);
    ASSERT(STMT_BLOCK(stmt).stmts[0]->kind == STMT_VAR_DECL);
    ASSERT(STMT_BLOCK(stmt).stmts[1]->kind == STMT_BLOCK);
    ASSERT(STMT_BLOCK(STMT_BLOCK(stmt).stmts[1]).count == 1);
    ASSERT(STMT_BLOCK(STMT_BLOCK(stmt).stmts[1]).stmts[0]->kind == STMT_VAR_DECL);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Parsing of a simple bit-field within a struct. */
static void test_parser_bitfield(void)
{
    const char *src = "struct S { unsigned f : 1; };";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_struct_decl(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_STRUCT_DECL);
    ASSERT(STMT_STRUCT_DECL(stmt).count == 1);
    ASSERT(strcmp(STMT_STRUCT_DECL(stmt).members[0].name, "f") == 0);
    ASSERT(STMT_STRUCT_DECL(stmt).members[0].bit_width == 1);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Ensure struct member parsing helpers work via parse_member_list */
static void test_parser_member_helpers(void)
{
    const char *src = "struct S { int a; int b[2]; unsigned c : 3; };";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_struct_decl(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_STRUCT_DECL);
    ASSERT(STMT_STRUCT_DECL(stmt).count == 3);
    ASSERT(strcmp(STMT_STRUCT_DECL(stmt).members[0].name, "a") == 0);
    ASSERT(STMT_STRUCT_DECL(stmt).members[1].type == TYPE_ARRAY);
    ASSERT(STMT_STRUCT_DECL(stmt).members[2].bit_width == 3);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

/* Parse a variable declaration using a struct tag. */
static void test_parser_struct_tag_var(void)
{
    const char *src = "struct S x;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    symtable_t funcs; symtable_init(&funcs);
    func_t *fn = NULL; stmt_t *global = NULL;
    ASSERT(parser_parse_toplevel(&p, &funcs, &fn, &global));
    ASSERT(fn == NULL);
    ASSERT(global && global->kind == STMT_VAR_DECL);
    ASSERT(STMT_VAR_DECL(global).type == TYPE_STRUCT);
    ASSERT(strcmp(STMT_VAR_DECL(global).tag, "S") == 0);
    symtable_free(&funcs);
    ast_free_stmt(global);
    lexer_free_tokens(toks, count);
}

/* Parse a function prototype using struct tags. */
static void test_parser_struct_tag_func(void)
{
    const char *src = "struct S foo(struct S a);";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    symtable_t funcs; symtable_init(&funcs);
    func_t *fn = NULL; stmt_t *global = NULL;
    ASSERT(parser_parse_toplevel(&p, &funcs, &fn, &global));
    ASSERT(fn == NULL && global == NULL);
    symbol_t *sym = symtable_lookup(&funcs, "foo");
    ASSERT(sym && sym->type == TYPE_STRUCT);
    ASSERT(sym->param_count == 1);
    ASSERT(sym->param_types[0] == TYPE_STRUCT);
    symtable_free(&funcs);
    lexer_free_tokens(toks, count);
}

/* Parse a function definition using struct tags. */
static void test_parser_struct_tag_func_def(void)
{
    const char *src = "struct S foo(struct S a) { return a; }";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    func_t *fn = parser_parse_func(&p, NULL, 0, 0);
    ASSERT(fn);
    ASSERT(fn->return_type == TYPE_STRUCT);
    ASSERT(strcmp(fn->return_tag, "S") == 0);
    ASSERT(fn->param_count == 1);
    ASSERT(fn->param_types[0] == TYPE_STRUCT);
    ASSERT(strcmp(fn->param_tags[0], "S") == 0);
    ast_free_func(fn);
    lexer_free_tokens(toks, count);
}

/* Verify that line directives influence token line/column fields. */
static void test_line_directive(void)
{
    const char *src = "# 5 \"file.c\"\nint x;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_KW_INT && toks[0].line == 5 && toks[0].column == 1);
    lexer_free_tokens(toks, count);
}

/* Verify escape sequences within character and string literals */
static void test_lexer_escapes(void)
{
    const char *src = "'\\r' '\\b' '\\f' '\\v' '\\123' '\\x7F'";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_CHAR && toks[0].lexeme[0] == '\r');
    ASSERT(toks[1].type == TOK_CHAR && toks[1].lexeme[0] == '\b');
    ASSERT(toks[2].type == TOK_CHAR && toks[2].lexeme[0] == '\f');
    ASSERT(toks[3].type == TOK_CHAR && toks[3].lexeme[0] == '\v');
    ASSERT(toks[4].type == TOK_CHAR && toks[4].lexeme[0] == 'S');
    ASSERT(toks[5].type == TOK_CHAR && (unsigned char)toks[5].lexeme[0] == 0x7F);
    lexer_free_tokens(toks, count);

    const char *str_src = "\"\\r\\b\\f\\v\\123\\x7F\"";
    toks = lexer_tokenize(str_src, &count);
    ASSERT(toks[0].type == TOK_STRING);
    ASSERT(strlen(toks[0].lexeme) == 6);
    ASSERT(toks[0].lexeme[0] == '\r');
    ASSERT(toks[0].lexeme[1] == '\b');
    ASSERT(toks[0].lexeme[2] == '\f');
    ASSERT(toks[0].lexeme[3] == '\v');
    ASSERT(toks[0].lexeme[4] == 'S');
    ASSERT((unsigned char)toks[0].lexeme[5] == 0x7F);
    lexer_free_tokens(toks, count);
}

/* Unterminated character constant should yield an error token */
static void test_lexer_char_missing_quote(void)
{
    const char *src = "'a";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_UNKNOWN);
    lexer_free_tokens(toks, count);
}

/* Unterminated string literal should yield an error token */
static void test_lexer_string_missing_quote(void)
{
    const char *src = "\"abc";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_UNKNOWN);
    lexer_free_tokens(toks, count);
}

/* Unterminated escape sequence should not crash the lexer */
static void test_lexer_truncated_escape(void)
{
    const char *char_src = "'\\";
    size_t count = 0;
    token_t *toks = lexer_tokenize(char_src, &count);
    ASSERT(toks[0].type == TOK_UNKNOWN);
    lexer_free_tokens(toks, count);

    const char *str_src = "\"\\";
    toks = lexer_tokenize(str_src, &count);
    ASSERT(toks[0].type == TOK_UNKNOWN);
    lexer_free_tokens(toks, count);
}

/* Octal escape values beyond 255 should be clamped */
static void test_lexer_octal_range(void)
{
    const char *src = "'\\400' \"\\400\"";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    ASSERT(toks[0].type == TOK_CHAR &&
           (unsigned char)toks[0].lexeme[0] == 255);
    ASSERT(toks[1].type == TOK_STRING && strlen(toks[1].lexeme) == 1 &&
           (unsigned char)toks[1].lexeme[0] == 255);
    lexer_free_tokens(toks, count);
}

/* Ensure the vector grows correctly for large element counts */
static void test_vector_large(void)
{
    const size_t big_count = 1000000;
    vector_t v;
    vector_init(&v, sizeof(int));
    for (size_t i = 0; i < big_count; i++) {
        ASSERT(vector_push(&v, &i));
    }
    ASSERT(v.count == big_count);
    for (size_t i = 0; i < big_count; i++) {
        ASSERT(((int *)v.data)[i] == (int)i);
    }
    vector_free(&v);
}

/* vector_push should fail when element size is zero */
static void test_vector_zero_elem_size(void)
{
    vector_t v;
    vector_init(&v, sizeof(int));
    /* simulate a zero-sized vector without triggering the assert */
    v.elem_size = 0;
    int x = 42;
    ASSERT(!vector_push(&v, &x));
    ASSERT(v.count == 0);
    vector_free(&v);
}

/*
 * Entry point for the test executable.  Each unit test is run in
 * sequence and the total number of failures reported at the end.
 */
int main(void)
{
    test_lexer_basic();
    test_lexer_comments();
    test_lexer_percent();
    test_lexer_new_types();
    test_lexer_complex_kw();
    test_lexer_imag_number();
    test_parser_imag_literal();
    test_parser_complex_literal();
    test_parser_expr();
    test_parser_stmt_return();
    test_parser_stmt_return_void();
    test_parser_var_decl_init();
    test_parser_short_decl();
    test_parser_bool_decl();
    test_parser__Bool_decl();
    test_parser_float_complex_decl();
    test_parser_double_complex_decl();
    test_parser_long_double_complex_decl();
    test_lexer_static_kw();
    test_parser_static_local();
    test_parser_array_decl();
    test_parser_index_expr();
    test_parser_unary_neg();
    test_parser_pointer_arith();
    test_parser_mod();
    test_parser_global_init();
    test_parser_unary_expr();
    test_parser_logical();
    test_parser_conditional();
    test_lexer_sizeof();
    test_parser_sizeof();
    test_parser_variadic_call();
    test_parser_func();
    test_parser_block();
    test_parser_bitfield();
    test_parser_member_helpers();
    test_parser_struct_tag_var();
    test_parser_struct_tag_func();
    test_parser_struct_tag_func_def();
    test_line_directive();
    test_lexer_escapes();
    test_lexer_char_missing_quote();
    test_lexer_string_missing_quote();
    test_lexer_truncated_escape();
    test_lexer_octal_range();
    test_vector_zero_elem_size();
    test_vector_large();
    if (failures == 0) {
        printf("All unit tests passed\n");
    } else {
        printf("%d unit test(s) failed\n", failures);
    }
    return failures ? 1 : 0;
}

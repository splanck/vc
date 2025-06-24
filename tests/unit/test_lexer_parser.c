#include <stdio.h>
#include <string.h>
#include "token.h"
#include "parser.h"
#include "ast.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

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

static void test_parser_expr(void)
{
    const char *src = "1 + 2 * 3";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr);
    ASSERT(expr->kind == EXPR_BINARY);
    ASSERT(expr->binary.op == BINOP_ADD);
    expr_t *left = expr->binary.left;
    expr_t *right = expr->binary.right;
    ASSERT(left && left->kind == EXPR_NUMBER && strcmp(left->number.value, "1") == 0);
    ASSERT(right && right->kind == EXPR_BINARY);
    ASSERT(right->binary.op == BINOP_MUL);
    ASSERT(right->binary.left->kind == EXPR_NUMBER && strcmp(right->binary.left->number.value, "2") == 0);
    ASSERT(right->binary.right->kind == EXPR_NUMBER && strcmp(right->binary.right->number.value, "3") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

static void test_parser_stmt_return(void)
{
    const char *src = "return 5;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_RETURN);
    ASSERT(stmt->ret.expr->kind == EXPR_NUMBER && strcmp(stmt->ret.expr->number.value, "5") == 0);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

static void test_parser_stmt_return_void(void)
{
    const char *src = "return;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_RETURN);
    ASSERT(stmt->ret.expr == NULL);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

static void test_parser_var_decl_init(void)
{
    const char *src = "int x = 5;";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_VAR_DECL);
    ASSERT(strcmp(stmt->var_decl.name, "x") == 0);
    ASSERT(stmt->var_decl.type == TYPE_INT);
    ASSERT(stmt->var_decl.init && stmt->var_decl.init->kind == EXPR_NUMBER &&
           strcmp(stmt->var_decl.init->number.value, "5") == 0);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

static void test_parser_array_decl(void)
{
    const char *src = "int a[4];";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_VAR_DECL);
    ASSERT(strcmp(stmt->var_decl.name, "a") == 0);
    ASSERT(stmt->var_decl.type == TYPE_ARRAY);
    ASSERT(stmt->var_decl.array_size == 4);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

static void test_parser_index_expr(void)
{
    const char *src = "a[1]";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr && expr->kind == EXPR_INDEX);
    ASSERT(expr->index.array->kind == EXPR_IDENT);
    ASSERT(strcmp(expr->index.array->ident.name, "a") == 0);
    ASSERT(expr->index.index->kind == EXPR_NUMBER &&
           strcmp(expr->index.index->number.value, "1") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

static void test_parser_unary_neg(void)
{
    const char *src = "-5";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *expr = parser_parse_expr(&p);
    ASSERT(expr);
    ASSERT(expr->kind == EXPR_UNARY);
    ASSERT(expr->unary.op == UNOP_NEG);
    ASSERT(expr->unary.operand->kind == EXPR_NUMBER &&
           strcmp(expr->unary.operand->number.value, "5") == 0);
    ast_free_expr(expr);
    lexer_free_tokens(toks, count);
}

static void test_parser_func(void)
{
    const char *src = "int main() { return 0; }";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    func_t *fn = parser_parse_func(&p);
    ASSERT(fn);
    ASSERT(strcmp(fn->name, "main") == 0);
    ASSERT(fn->return_type == TYPE_INT);
    ASSERT(fn->body_count == 1);
    ASSERT(fn->body[0]->kind == STMT_RETURN);
    ast_free_func(fn);
    lexer_free_tokens(toks, count);
}

static void test_parser_block(void)
{
    const char *src = "{ int x; { int y; } }";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    stmt_t *stmt = parser_parse_stmt(&p);
    ASSERT(stmt);
    ASSERT(stmt->kind == STMT_BLOCK);
    ASSERT(stmt->block.count == 2);
    ASSERT(stmt->block.stmts[0]->kind == STMT_VAR_DECL);
    ASSERT(stmt->block.stmts[1]->kind == STMT_BLOCK);
    ASSERT(stmt->block.stmts[1]->block.count == 1);
    ASSERT(stmt->block.stmts[1]->block.stmts[0]->kind == STMT_VAR_DECL);
    ast_free_stmt(stmt);
    lexer_free_tokens(toks, count);
}

int main(void)
{
    test_lexer_basic();
    test_lexer_comments();
    test_parser_expr();
    test_parser_stmt_return();
    test_parser_stmt_return_void();
    test_parser_var_decl_init();
    test_parser_array_decl();
    test_parser_index_expr();
    test_parser_unary_neg();
    test_parser_func();
    test_parser_block();
    if (failures == 0) {
        printf("All unit tests passed\n");
    } else {
        printf("%d unit test(s) failed\n", failures);
    }
    return failures ? 1 : 0;
}

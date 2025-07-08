#include <stdio.h>
#include <string.h>
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "ast.h"
#include "ast_expr.h"
#include "symtable.h"
#include "semantic_expr.h"
#include "ir_core.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

static void test_parser_cast_expr(void)
{
    const char *src = "(int)3.5";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *e = parser_parse_expr(&p);
    ASSERT(e);
    ASSERT(e->kind == EXPR_CAST);
    ASSERT(e->cast.type == TYPE_INT);
    ASSERT(e->cast.expr && e->cast.expr->kind == EXPR_NUMBER);
    ast_free_expr(e);
    lexer_free_tokens(toks, count);
}

static void test_ir_cast_expr(void)
{
    const char *src = "(int)3.5";
    size_t count = 0;
    token_t *toks = lexer_tokenize(src, &count);
    parser_t p; parser_init(&p, toks, count);
    expr_t *e = parser_parse_expr(&p);
    ir_builder_t ir; ir_builder_init(&ir);
    symtable_t vars, funcs; symtable_init(&vars); symtable_init(&funcs);
    ir_value_t val; type_kind_t t = check_expr(e, &vars, &funcs, &ir, &val);
    ASSERT(t == TYPE_INT);
    ASSERT(ir.head && ir.head->op == IR_CONST && ir.head->imm == 3);
    ASSERT(ir.head->next == NULL);
    ast_free_expr(e);
    lexer_free_tokens(toks, count);
    ir_builder_free(&ir);
    symtable_free(&vars); symtable_free(&funcs);
}

int main(void)
{
    test_parser_cast_expr();
    test_ir_cast_expr();
    if (failures == 0)
        printf("All cast_expr tests passed\n");
    else
        printf("%d cast_expr test(s) failed\n", failures);
    return failures ? 1 : 0;
}

/*
 * Control flow expression constructors.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ast_expr.h"
#include "util.h"

static expr_t *new_expr(expr_kind_t kind, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = kind;
    expr->line = line;
    expr->column = column;
    return expr;
}

expr_t *ast_make_cond(expr_t *cond, expr_t *then_expr, expr_t *else_expr,
                      size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_COND, line, column);
    if (!expr)
        return NULL;
    expr->data.cond.cond = cond;
    expr->data.cond.then_expr = then_expr;
    expr->data.cond.else_expr = else_expr;
    return expr;
}

expr_t *ast_make_call(const char *name, expr_t **args, size_t arg_count,
                      size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_CALL, line, column);
    if (!expr)
        return NULL;
    expr->data.call.name = vc_strdup(name ? name : "");
    if (!expr->data.call.name) {
        free(expr);
        return NULL;
    }
    expr->data.call.args = args;
    expr->data.call.arg_count = arg_count;
    return expr;
}


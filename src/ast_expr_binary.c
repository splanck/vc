/*
 * Binary and unary expression constructors.
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

expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right,
                        size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_BINARY, line, column);
    if (!expr)
        return NULL;
    expr->data.binary.op = op;
    expr->data.binary.left = left;
    expr->data.binary.right = right;
    return expr;
}

expr_t *ast_make_unary(unop_t op, expr_t *operand,
                       size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_UNARY, line, column);
    if (!expr)
        return NULL;
    expr->data.unary.op = op;
    expr->data.unary.operand = operand;
    return expr;
}

expr_t *ast_make_assign(const char *name, expr_t *value,
                        size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_ASSIGN, line, column);
    if (!expr)
        return NULL;
    expr->data.assign.name = vc_strdup(name ? name : "");
    if (!expr->data.assign.name) {
        free(expr);
        return NULL;
    }
    expr->data.assign.value = value;
    return expr;
}

expr_t *ast_make_index(expr_t *array, expr_t *index,
                       size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_INDEX, line, column);
    if (!expr)
        return NULL;
    expr->data.index.array = array;
    expr->data.index.index = index;
    return expr;
}

expr_t *ast_make_assign_index(expr_t *array, expr_t *index, expr_t *value,
                              size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_ASSIGN_INDEX, line, column);
    if (!expr)
        return NULL;
    expr->data.assign_index.array = array;
    expr->data.assign_index.index = index;
    expr->data.assign_index.value = value;
    return expr;
}

expr_t *ast_make_assign_member(expr_t *object, const char *member, expr_t *value,
                               int via_ptr, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_ASSIGN_MEMBER, line, column);
    if (!expr)
        return NULL;
    expr->data.assign_member.object = object;
    expr->data.assign_member.member = vc_strdup(member ? member : "");
    if (!expr->data.assign_member.member) {
        free(expr);
        return NULL;
    }
    expr->data.assign_member.value = value;
    expr->data.assign_member.via_ptr = via_ptr;
    return expr;
}

expr_t *ast_make_member(expr_t *object, const char *member, int via_ptr,
                        size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_MEMBER, line, column);
    if (!expr)
        return NULL;
    expr->data.member.object = object;
    expr->data.member.member = vc_strdup(member ? member : "");
    if (!expr->data.member.member) {
        free(expr);
        return NULL;
    }
    expr->data.member.via_ptr = via_ptr;
    return expr;
}


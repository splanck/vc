/*
 * Type and compound expression constructors.
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

expr_t *ast_make_sizeof_type(type_kind_t type, size_t array_size,
                             size_t elem_size, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_SIZEOF, line, column);
    if (!expr)
        return NULL;
    expr->data.sizeof_expr.is_type = 1;
    expr->data.sizeof_expr.type = type;
    expr->data.sizeof_expr.array_size = array_size;
    expr->data.sizeof_expr.elem_size = elem_size;
    expr->data.sizeof_expr.expr = NULL;
    return expr;
}

expr_t *ast_make_sizeof_expr(expr_t *e, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_SIZEOF, line, column);
    if (!expr)
        return NULL;
    expr->data.sizeof_expr.is_type = 0;
    expr->data.sizeof_expr.type = TYPE_UNKNOWN;
    expr->data.sizeof_expr.array_size = 0;
    expr->data.sizeof_expr.elem_size = 0;
    expr->data.sizeof_expr.expr = e;
    return expr;
}

expr_t *ast_make_offsetof(type_kind_t type, const char *tag,
                          char **members, size_t member_count,
                          size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_OFFSETOF, line, column);
    if (!expr)
        return NULL;
    expr->data.offsetof_expr.type = type;
    expr->data.offsetof_expr.tag = vc_strdup(tag ? tag : "");
    if (!expr->data.offsetof_expr.tag) {
        free(expr);
        return NULL;
    }
    expr->data.offsetof_expr.members = members;
    expr->data.offsetof_expr.member_count = member_count;
    return expr;
}

expr_t *ast_make_alignof_type(type_kind_t type, size_t array_size,
                              size_t elem_size, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_ALIGNOF, line, column);
    if (!expr)
        return NULL;
    expr->data.alignof_expr.is_type = 1;
    expr->data.alignof_expr.type = type;
    expr->data.alignof_expr.array_size = array_size;
    expr->data.alignof_expr.elem_size = elem_size;
    expr->data.alignof_expr.expr = NULL;
    return expr;
}

expr_t *ast_make_alignof_expr(expr_t *e, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_ALIGNOF, line, column);
    if (!expr)
        return NULL;
    expr->data.alignof_expr.is_type = 0;
    expr->data.alignof_expr.type = TYPE_UNKNOWN;
    expr->data.alignof_expr.array_size = 0;
    expr->data.alignof_expr.elem_size = 0;
    expr->data.alignof_expr.expr = e;
    return expr;
}

expr_t *ast_make_cast(type_kind_t type, size_t array_size, size_t elem_size,
                      expr_t *e, size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_CAST, line, column);
    if (!expr)
        return NULL;
    expr->data.cast.type = type;
    expr->data.cast.array_size = array_size;
    expr->data.cast.elem_size = elem_size;
    expr->data.cast.expr = e;
    return expr;
}

expr_t *ast_make_compound(type_kind_t type, size_t array_size,
                          size_t elem_size, expr_t *init,
                          init_entry_t *init_list, size_t init_count,
                          size_t line, size_t column)
{
    expr_t *expr = new_expr(EXPR_COMPLIT, line, column);
    if (!expr)
        return NULL;
    expr->data.compound.type = type;
    expr->data.compound.array_size = array_size;
    expr->data.compound.elem_size = elem_size;
    expr->data.compound.init = init;
    expr->data.compound.init_list = init_list;
    expr->data.compound.init_count = init_count;
    return expr;
}


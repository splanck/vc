/*
 * Expression helpers and destructor.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ast_expr.h"

/* Recursively free an expression node and its children. */
void ast_free_expr(expr_t *expr)
{
    if (!expr)
        return;
    switch (expr->kind) {
    case EXPR_NUMBER:
        free(expr->data.number.value);
        break;
    case EXPR_IDENT:
        free(expr->data.ident.name);
        break;
    case EXPR_STRING:
        free(expr->data.string.value);
        break;
    case EXPR_CHAR:
        break;
    case EXPR_COMPLEX_LITERAL:
        break;
    case EXPR_UNARY:
        ast_free_expr(expr->data.unary.operand);
        break;
    case EXPR_BINARY:
        ast_free_expr(expr->data.binary.left);
        ast_free_expr(expr->data.binary.right);
        break;
    case EXPR_COND:
        ast_free_expr(expr->data.cond.cond);
        ast_free_expr(expr->data.cond.then_expr);
        ast_free_expr(expr->data.cond.else_expr);
        break;
    case EXPR_ASSIGN:
        free(expr->data.assign.name);
        ast_free_expr(expr->data.assign.value);
        break;
    case EXPR_INDEX:
        ast_free_expr(expr->data.index.array);
        ast_free_expr(expr->data.index.index);
        break;
    case EXPR_ASSIGN_INDEX:
        ast_free_expr(expr->data.assign_index.array);
        ast_free_expr(expr->data.assign_index.index);
        ast_free_expr(expr->data.assign_index.value);
        break;
    case EXPR_ASSIGN_MEMBER:
        ast_free_expr(expr->data.assign_member.object);
        free(expr->data.assign_member.member);
        ast_free_expr(expr->data.assign_member.value);
        break;
    case EXPR_MEMBER:
        ast_free_expr(expr->data.member.object);
        free(expr->data.member.member);
        break;
    case EXPR_SIZEOF:
        if (!expr->data.sizeof_expr.is_type)
            ast_free_expr(expr->data.sizeof_expr.expr);
        break;
    case EXPR_OFFSETOF:
        for (size_t i = 0; i < expr->data.offsetof_expr.member_count; i++)
            free(expr->data.offsetof_expr.members[i]);
        free(expr->data.offsetof_expr.members);
        free(expr->data.offsetof_expr.tag);
        break;
    case EXPR_ALIGNOF:
        if (!expr->data.alignof_expr.is_type)
            ast_free_expr(expr->data.alignof_expr.expr);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < expr->data.call.arg_count; i++)
            ast_free_expr(expr->data.call.args[i]);
        free(expr->data.call.args);
        free(expr->data.call.name);
        break;
    case EXPR_CAST:
        ast_free_expr(expr->data.cast.expr);
        break;
    case EXPR_COMPLIT:
        ast_free_expr(expr->data.compound.init);
        for (size_t i = 0; i < expr->data.compound.init_count; i++) {
            ast_free_expr(expr->data.compound.init_list[i].index);
            ast_free_expr(expr->data.compound.init_list[i].value);
            free(expr->data.compound.init_list[i].field);
        }
        free(expr->data.compound.init_list);
        break;
    }
    free(expr);
}


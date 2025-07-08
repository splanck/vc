/*
 * AST constructors and helpers for the compiler.
 *
 * This module contains the concrete implementations of the expression
 * constructor functions declared in ``ast_expr.h''.  Each routine allocates
 * a new node, fills in its tagged union fields and returns the result to
 * the caller.  ``ast_free_expr'' provides the matching destructor which
 * recursively frees a tree of expressions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ast_expr.h"
#include "util.h"
#include <string.h>

static char *strip_suffix(const char *tok, int *is_unsigned, int *long_count)
{
    *is_unsigned = 0;
    *long_count = 0;
    size_t len = strlen(tok);
    size_t i = len;
    while (i > 0) {
        char c = tok[i-1];
        if (c == 'u' || c == 'U') {
            *is_unsigned = 1;
            i--;
        } else if (c == 'l' || c == 'L') {
            i--;
            (*long_count)++;
            if (i > 0 && (tok[i-1] == 'l' || tok[i-1] == 'L')) {
                (*long_count)++;
                i--;
            }
        } else {
            break;
        }
    }
    return vc_strndup(tok, i);
}
/* Constructors for expressions */
/* Create a numeric literal expression node. */
expr_t *ast_make_number(const char *value, size_t line, size_t column)
{
    int is_unsigned, long_count;
    char *val = strip_suffix(value ? value : "", &is_unsigned, &long_count);
    if (!val)
        return NULL;
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr) {
        free(val);
        return NULL;
    }
    expr->kind = EXPR_NUMBER;
    expr->line = line;
    expr->column = column;
    expr->number.value = val;
    expr->number.is_unsigned = is_unsigned;
    expr->number.long_count = long_count;
    return expr;
}

/* Create an identifier expression node. */
expr_t *ast_make_ident(const char *name, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_IDENT;
    expr->line = line;
    expr->column = column;
    expr->ident.name = vc_strdup(name ? name : "");
    if (!expr->ident.name) {
        free(expr);
        return NULL;
    }
    return expr;
}

/* Create a string literal expression node. */
static expr_t *make_string(const char *value, size_t line, size_t column, int is_wide)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_STRING;
    expr->line = line;
    expr->column = column;
    expr->string.value = vc_strdup(value ? value : "");
    if (!expr->string.value) {
        free(expr);
        return NULL;
    }
    expr->string.is_wide = is_wide;
    return expr;
}

expr_t *ast_make_string(const char *value, size_t line, size_t column)
{
    return make_string(value, line, column, 0);
}

expr_t *ast_make_wstring(const char *value, size_t line, size_t column)
{
    return make_string(value, line, column, 1);
}

/* Create a character literal expression node. */
static expr_t *make_char(char value, size_t line, size_t column, int is_wide)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_CHAR;
    expr->line = line;
    expr->column = column;
    expr->ch.value = value;
    expr->ch.is_wide = is_wide;
    return expr;
}

expr_t *ast_make_char(char value, size_t line, size_t column)
{
    return make_char(value, line, column, 0);
}

expr_t *ast_make_wchar(char value, size_t line, size_t column)
{
    return make_char(value, line, column, 1);
}

/* Create a binary operation expression node. */
expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right,
                        size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_BINARY;
    expr->line = line;
    expr->column = column;
    expr->binary.op = op;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

/* Create a unary operation expression node. */
expr_t *ast_make_unary(unop_t op, expr_t *operand,
                       size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_UNARY;
    expr->line = line;
    expr->column = column;
    expr->unary.op = op;
    expr->unary.operand = operand;
    return expr;
}

/* Create a conditional expression node. */
expr_t *ast_make_cond(expr_t *cond, expr_t *then_expr, expr_t *else_expr,
                      size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_COND;
    expr->line = line;
    expr->column = column;
    expr->cond.cond = cond;
    expr->cond.then_expr = then_expr;
    expr->cond.else_expr = else_expr;
    return expr;
}

/* Create an assignment expression node assigning to \p name. */
expr_t *ast_make_assign(const char *name, expr_t *value,
                        size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_ASSIGN;
    expr->line = line;
    expr->column = column;
    expr->assign.name = vc_strdup(name ? name : "");
    if (!expr->assign.name) {
        free(expr);
        return NULL;
    }
    expr->assign.value = value;
    return expr;
}

/* Create an array indexing expression node. */
expr_t *ast_make_index(expr_t *array, expr_t *index,
                       size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_INDEX;
    expr->line = line;
    expr->column = column;
    expr->index.array = array;
    expr->index.index = index;
    return expr;
}

/* Create an array element assignment expression node. */
expr_t *ast_make_assign_index(expr_t *array, expr_t *index, expr_t *value,
                              size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_ASSIGN_INDEX;
    expr->line = line;
    expr->column = column;
    expr->assign_index.array = array;
    expr->assign_index.index = index;
    expr->assign_index.value = value;
    return expr;
}

/* Create a union/struct member assignment expression node. */
expr_t *ast_make_assign_member(expr_t *object, const char *member, expr_t *value,
                               int via_ptr, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_ASSIGN_MEMBER;
    expr->line = line;
    expr->column = column;
    expr->assign_member.object = object;
    expr->assign_member.member = vc_strdup(member ? member : "");
    if (!expr->assign_member.member) {
        free(expr);
        return NULL;
    }
    expr->assign_member.value = value;
    expr->assign_member.via_ptr = via_ptr;
    return expr;
}

/* Create a member access expression node. */
expr_t *ast_make_member(expr_t *object, const char *member, int via_ptr,
                        size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_MEMBER;
    expr->line = line;
    expr->column = column;
    expr->member.object = object;
    expr->member.member = vc_strdup(member ? member : "");
    if (!expr->member.member) {
        free(expr);
        return NULL;
    }
    expr->member.via_ptr = via_ptr;
    return expr;
}

/* Create a sizeof expression for a type. */
expr_t *ast_make_sizeof_type(type_kind_t type, size_t array_size,
                             size_t elem_size, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_SIZEOF;
    expr->line = line;
    expr->column = column;
    expr->sizeof_expr.is_type = 1;
    expr->sizeof_expr.type = type;
    expr->sizeof_expr.array_size = array_size;
    expr->sizeof_expr.elem_size = elem_size;
    expr->sizeof_expr.expr = NULL;
    return expr;
}

/* Create a sizeof expression for another expression. */
expr_t *ast_make_sizeof_expr(expr_t *e, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_SIZEOF;
    expr->line = line;
    expr->column = column;
    expr->sizeof_expr.is_type = 0;
    expr->sizeof_expr.type = TYPE_UNKNOWN;
    expr->sizeof_expr.array_size = 0;
    expr->sizeof_expr.elem_size = 0;
    expr->sizeof_expr.expr = e;
    return expr;
}

/* Create an offsetof expression node. */
expr_t *ast_make_offsetof(type_kind_t type, const char *tag,
                          char **members, size_t member_count,
                          size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_OFFSETOF;
    expr->line = line;
    expr->column = column;
    expr->offsetof_expr.type = type;
    expr->offsetof_expr.tag = vc_strdup(tag ? tag : "");
    if (!expr->offsetof_expr.tag) {
        free(expr);
        return NULL;
    }
    expr->offsetof_expr.members = members;
    expr->offsetof_expr.member_count = member_count;
    return expr;
}

/* Create a type cast expression node. */
expr_t *ast_make_cast(type_kind_t type, size_t array_size, size_t elem_size,
                      expr_t *e, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_CAST;
    expr->line = line;
    expr->column = column;
    expr->cast.type = type;
    expr->cast.array_size = array_size;
    expr->cast.elem_size = elem_size;
    expr->cast.expr = e;
    return expr;
}

/* Create a function call expression node. */
expr_t *ast_make_call(const char *name, expr_t **args, size_t arg_count,
                      size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_CALL;
    expr->line = line;
    expr->column = column;
    expr->call.name = vc_strdup(name ? name : "");
    if (!expr->call.name) {
        free(expr);
        return NULL;
    }
    expr->call.args = args;
    expr->call.arg_count = arg_count;
    return expr;
}

/* Create a compound literal expression node. */
expr_t *ast_make_compound(type_kind_t type, size_t array_size,
                          size_t elem_size, expr_t *init,
                          init_entry_t *init_list, size_t init_count,
                          size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_COMPLIT;
    expr->line = line;
    expr->column = column;
    expr->compound.type = type;
    expr->compound.array_size = array_size;
    expr->compound.elem_size = elem_size;
    expr->compound.init = init;
    expr->compound.init_list = init_list;
    expr->compound.init_count = init_count;
    return expr;
}

/* Destructors */
/* Recursively free an expression node and its children. */
void ast_free_expr(expr_t *expr)
{
    if (!expr)
        return;
    switch (expr->kind) {
    case EXPR_NUMBER:
        free(expr->number.value);
        break;
    case EXPR_IDENT:
        free(expr->ident.name);
        break;
    case EXPR_STRING:
        free(expr->string.value);
        break;
    case EXPR_CHAR:
        break;
    case EXPR_UNARY:
        ast_free_expr(expr->unary.operand);
        break;
    case EXPR_BINARY:
        ast_free_expr(expr->binary.left);
        ast_free_expr(expr->binary.right);
        break;
    case EXPR_COND:
        ast_free_expr(expr->cond.cond);
        ast_free_expr(expr->cond.then_expr);
        ast_free_expr(expr->cond.else_expr);
        break;
    case EXPR_ASSIGN:
        free(expr->assign.name);
        ast_free_expr(expr->assign.value);
        break;
    case EXPR_INDEX:
        ast_free_expr(expr->index.array);
        ast_free_expr(expr->index.index);
        break;
    case EXPR_ASSIGN_INDEX:
        ast_free_expr(expr->assign_index.array);
        ast_free_expr(expr->assign_index.index);
        ast_free_expr(expr->assign_index.value);
        break;
    case EXPR_ASSIGN_MEMBER:
        ast_free_expr(expr->assign_member.object);
        free(expr->assign_member.member);
        ast_free_expr(expr->assign_member.value);
        break;
    case EXPR_MEMBER:
        ast_free_expr(expr->member.object);
        free(expr->member.member);
        break;
    case EXPR_SIZEOF:
        if (!expr->sizeof_expr.is_type)
            ast_free_expr(expr->sizeof_expr.expr);
        break;
    case EXPR_OFFSETOF:
        for (size_t i = 0; i < expr->offsetof_expr.member_count; i++)
            free(expr->offsetof_expr.members[i]);
        free(expr->offsetof_expr.members);
        free(expr->offsetof_expr.tag);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < expr->call.arg_count; i++)
            ast_free_expr(expr->call.args[i]);
        free(expr->call.args);
        free(expr->call.name);
        break;
    case EXPR_CAST:
        ast_free_expr(expr->cast.expr);
        break;
    case EXPR_COMPLIT:
        ast_free_expr(expr->compound.init);
        for (size_t i = 0; i < expr->compound.init_count; i++) {
            ast_free_expr(expr->compound.init_list[i].index);
            ast_free_expr(expr->compound.init_list[i].value);
            free(expr->compound.init_list[i].field);
        }
        free(expr->compound.init_list);
        break;
    }
    free(expr);
}


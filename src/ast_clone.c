/*
 * Routines for cloning AST expression nodes.
 *
 * Expression nodes are dynamically allocated and linked together to form
 * a tree.  Cloning must therefore walk the entire tree and duplicate each
 * node so that the clone shares no storage with the original.  This file
 * implements ``clone_expr'' which dispatches to helper functions for every
 * expression kind defined in ``ast.h''.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ast_clone.h"
#include "ast_expr.h"
#include "util.h"

/* Helper functions for cloning each expression kind. Each returns a newly
 * allocated node on success or NULL on allocation failure. */

/* Duplicate a numeric literal expression node. */
static expr_t *clone_number(const expr_t *expr)
{
    expr_t *n = malloc(sizeof(*n));
    if (!n)
        return NULL;
    n->kind = EXPR_NUMBER;
    n->line = expr->line;
    n->column = expr->column;
    n->data.number.value = vc_strdup(expr->data.number.value);
    if (!n->data.number.value) {
        free(n);
        return NULL;
    }
    n->data.number.is_unsigned = expr->data.number.is_unsigned;
    n->data.number.long_count = expr->data.number.long_count;
    return n;
}

/* Duplicate an identifier expression node. */
static expr_t *clone_ident(const expr_t *expr)
{
    return ast_make_ident(expr->data.ident.name, expr->line, expr->column);
}

/* Duplicate a string literal expression node. */
static expr_t *clone_string(const expr_t *expr)
{
    if (expr->data.string.is_wide)
        return ast_make_wstring(expr->data.string.value, expr->line, expr->column);
    return ast_make_string(expr->data.string.value, expr->line, expr->column);
}

/* Duplicate a character literal expression node. */
static expr_t *clone_char(const expr_t *expr)
{
    if (expr->data.ch.is_wide)
        return ast_make_wchar(expr->data.ch.value, expr->line, expr->column);
    return ast_make_char(expr->data.ch.value, expr->line, expr->column);
}

/* Duplicate a complex literal expression node. */
static expr_t *clone_complex_literal(const expr_t *expr)
{
    return ast_make_complex_literal(expr->data.complex_lit.real,
                                    expr->data.complex_lit.imag,
                                    expr->line, expr->column);
}

/* Clone a unary operation and its operand. */
static expr_t *clone_unary(const expr_t *expr)
{
    expr_t *op = clone_expr(expr->data.unary.operand);
    if (!op)
        return NULL;
    return ast_make_unary(expr->data.unary.op, op, expr->line, expr->column);
}

/* Clone a binary operation and its operands. */
static expr_t *clone_binary(const expr_t *expr)
{
    expr_t *l = clone_expr(expr->data.binary.left);
    expr_t *r = clone_expr(expr->data.binary.right);
    if (!l || !r) {
        ast_free_expr(l);
        ast_free_expr(r);
        return NULL;
    }
    return ast_make_binary(expr->data.binary.op, l, r, expr->line, expr->column);
}

/* Clone a conditional expression and its branches. */
static expr_t *clone_cond(const expr_t *expr)
{
    expr_t *c = clone_expr(expr->data.cond.cond);
    expr_t *t = clone_expr(expr->data.cond.then_expr);
    expr_t *e = clone_expr(expr->data.cond.else_expr);
    if (!c || !t || !e) {
        ast_free_expr(c);
        ast_free_expr(t);
        ast_free_expr(e);
        return NULL;
    }
    return ast_make_cond(c, t, e, expr->line, expr->column);
}

/* Clone an assignment expression. */
static expr_t *clone_assign(const expr_t *expr)
{
    expr_t *v = clone_expr(expr->data.assign.value);
    if (!v)
        return NULL;
    return ast_make_assign(expr->data.assign.name, v, expr->line, expr->column);
}

/* Clone an array indexing expression. */
static expr_t *clone_index(const expr_t *expr)
{
    expr_t *a = clone_expr(expr->data.index.array);
    expr_t *i = clone_expr(expr->data.index.index);
    if (!a || !i) {
        ast_free_expr(a);
        ast_free_expr(i);
        return NULL;
    }
    return ast_make_index(a, i, expr->line, expr->column);
}

/* Clone an array element assignment. */
static expr_t *clone_assign_index(const expr_t *expr)
{
    expr_t *a = clone_expr(expr->data.assign_index.array);
    expr_t *i = clone_expr(expr->data.assign_index.index);
    expr_t *v = clone_expr(expr->data.assign_index.value);
    if (!a || !i || !v) {
        ast_free_expr(a);
        ast_free_expr(i);
        ast_free_expr(v);
        return NULL;
    }
    return ast_make_assign_index(a, i, v, expr->line, expr->column);
}

/* Clone a struct or union member assignment. */
static expr_t *clone_assign_member(const expr_t *expr)
{
    expr_t *obj = clone_expr(expr->data.assign_member.object);
    expr_t *val = clone_expr(expr->data.assign_member.value);
    if (!obj || !val) {
        ast_free_expr(obj);
        ast_free_expr(val);
        return NULL;
    }
    return ast_make_assign_member(obj, expr->data.assign_member.member, val,
                                  expr->data.assign_member.via_ptr, expr->line,
                                  expr->column);
}

/* Clone a member access expression. */
static expr_t *clone_member(const expr_t *expr)
{
    expr_t *obj = clone_expr(expr->data.member.object);
    if (!obj)
        return NULL;
    return ast_make_member(obj, expr->data.member.member, expr->data.member.via_ptr,
                           expr->line, expr->column);
}

/* Clone a sizeof expression. */
static expr_t *clone_sizeof(const expr_t *expr)
{
    if (expr->data.sizeof_expr.is_type)
        return ast_make_sizeof_type(expr->data.sizeof_expr.type,
                                    expr->data.sizeof_expr.array_size,
                                    expr->data.sizeof_expr.elem_size, expr->line,
                                    expr->column);
    expr_t *e = clone_expr(expr->data.sizeof_expr.expr);
    if (!e)
        return NULL;
    return ast_make_sizeof_expr(e, expr->line, expr->column);
}

/* Clone an offsetof expression. */
static expr_t *clone_offsetof(const expr_t *expr)
{
    size_t n = expr->data.offsetof_expr.member_count;
    char **members = NULL;
    if (n) {
        members = malloc(n * sizeof(char *));
        if (!members)
            return NULL;
        for (size_t i = 0; i < n; i++) {
            members[i] = vc_strdup(expr->data.offsetof_expr.members[i]);
            if (!members[i]) {
                for (size_t j = 0; j < i; j++)
                    free(members[j]);
                free(members);
                return NULL;
            }
        }
    }
    char *tag = vc_strdup(expr->data.offsetof_expr.tag);
    if (!tag) {
        for (size_t i = 0; i < n; i++)
            free(members[i]);
        free(members);
        return NULL;
    }
    return ast_make_offsetof(expr->data.offsetof_expr.type, tag,
                             members, n, expr->line, expr->column);
}

/* Clone an alignof expression. */
static expr_t *clone_alignof(const expr_t *expr)
{
    if (expr->data.alignof_expr.is_type)
        return ast_make_alignof_type(expr->data.alignof_expr.type,
                                     expr->data.alignof_expr.array_size,
                                     expr->data.alignof_expr.elem_size,
                                     expr->line, expr->column);
    expr_t *e = clone_expr(expr->data.alignof_expr.expr);
    if (!e)
        return NULL;
    return ast_make_alignof_expr(e, expr->line, expr->column);
}

/* Clone a cast expression. */
static expr_t *clone_cast(const expr_t *expr)
{
    expr_t *inner = clone_expr(expr->data.cast.expr);
    if (!inner)
        return NULL;
    return ast_make_cast(expr->data.cast.type, expr->data.cast.array_size,
                         expr->data.cast.elem_size, inner,
                         expr->line, expr->column);
}

/* Clone a function call expression and its arguments. */
static expr_t *clone_call(const expr_t *expr)
{
    size_t n = expr->data.call.arg_count;
    expr_t **args = NULL;
    if (n) {
        args = malloc(n * sizeof(*args));
        if (!args)
            return NULL;
        for (size_t i = 0; i < n; i++) {
            args[i] = clone_expr(expr->data.call.args[i]);
            if (!args[i]) {
                for (size_t j = 0; j < i; j++)
                    ast_free_expr(args[j]);
                free(args);
                return NULL;
            }
        }
    }
    return ast_make_call(expr->data.call.name, args, n, expr->line, expr->column);
}

/* Clone a compound literal expression and its initializer list. */
static expr_t *clone_complit(const expr_t *expr)
{
    expr_t *init = clone_expr(expr->data.compound.init);
    init_entry_t *list = NULL;
    if (expr->data.compound.init_count) {
        list = malloc(expr->data.compound.init_count * sizeof(*list));
        if (!list) {
            ast_free_expr(init);
            return NULL;
        }
        for (size_t i = 0; i < expr->data.compound.init_count; i++) {
            list[i].kind = expr->data.compound.init_list[i].kind;
            list[i].field = expr->data.compound.init_list[i].field ?
                            vc_strdup(expr->data.compound.init_list[i].field) : NULL;
            list[i].index = clone_expr(expr->data.compound.init_list[i].index);
            list[i].value = clone_expr(expr->data.compound.init_list[i].value);
            if ((expr->data.compound.init_list[i].field && !list[i].field) ||
                (expr->data.compound.init_list[i].index && !list[i].index) ||
                (expr->data.compound.init_list[i].value && !list[i].value)) {
                for (size_t j = 0; j <= i; j++) {
                    free(list[j].field);
                    ast_free_expr(list[j].index);
                    ast_free_expr(list[j].value);
                }
                free(list);
                ast_free_expr(init);
                return NULL;
            }
        }
    }
    return ast_make_compound(expr->data.compound.type, expr->data.compound.array_size,
                             expr->data.compound.elem_size, init, list,
                             expr->data.compound.init_count, expr->line,
                             expr->column);
}

/* Recursively clone an expression tree by dispatching
 * to the helper functions above. Each clone_* function allocates
 * new nodes for its children, so the returned tree shares no state
 * with the original. NULL is returned if any allocation fails. */
expr_t *clone_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;
    switch (expr->kind) {
    case EXPR_NUMBER:
        return clone_number(expr);
    case EXPR_IDENT:
        return clone_ident(expr);
    case EXPR_STRING:
        return clone_string(expr);
    case EXPR_CHAR:
        return clone_char(expr);
    case EXPR_COMPLEX_LITERAL:
        return clone_complex_literal(expr);
    case EXPR_UNARY:
        return clone_unary(expr);
    case EXPR_BINARY:
        return clone_binary(expr);
    case EXPR_COND:
        return clone_cond(expr);
    case EXPR_ASSIGN:
        return clone_assign(expr);
    case EXPR_CALL:
        return clone_call(expr);
    case EXPR_INDEX:
        return clone_index(expr);
    case EXPR_ASSIGN_INDEX:
        return clone_assign_index(expr);
    case EXPR_ASSIGN_MEMBER:
        return clone_assign_member(expr);
    case EXPR_MEMBER:
        return clone_member(expr);
    case EXPR_SIZEOF:
        return clone_sizeof(expr);
    case EXPR_OFFSETOF:
        return clone_offsetof(expr);
    case EXPR_ALIGNOF:
        return clone_alignof(expr);
    case EXPR_CAST:
        return clone_cast(expr);
    case EXPR_COMPLIT:
        return clone_complit(expr);
    }
    return NULL;
}

#include <stdlib.h>
#include "ast_clone.h"
#include "util.h"

expr_t *clone_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;
    switch (expr->kind) {
    case EXPR_NUMBER:
        return ast_make_number(expr->number.value, expr->line, expr->column);
    case EXPR_IDENT:
        return ast_make_ident(expr->ident.name, expr->line, expr->column);
    case EXPR_STRING:
        return ast_make_string(expr->string.value, expr->line, expr->column);
    case EXPR_CHAR:
        return ast_make_char(expr->ch.value, expr->line, expr->column);
    case EXPR_UNARY: {
        expr_t *op = clone_expr(expr->unary.operand);
        if (!op)
            return NULL;
        return ast_make_unary(expr->unary.op, op, expr->line, expr->column);
    }
    case EXPR_BINARY: {
        expr_t *l = clone_expr(expr->binary.left);
        expr_t *r = clone_expr(expr->binary.right);
        if (!l || !r) {
            ast_free_expr(l);
            ast_free_expr(r);
            return NULL;
        }
        return ast_make_binary(expr->binary.op, l, r,
                               expr->line, expr->column);
    }
    case EXPR_COND: {
        expr_t *c = clone_expr(expr->cond.cond);
        expr_t *t = clone_expr(expr->cond.then_expr);
        expr_t *e = clone_expr(expr->cond.else_expr);
        if (!c || !t || !e) {
            ast_free_expr(c);
            ast_free_expr(t);
            ast_free_expr(e);
            return NULL;
        }
        return ast_make_cond(c, t, e, expr->line, expr->column);
    }
    case EXPR_ASSIGN: {
        expr_t *v = clone_expr(expr->assign.value);
        if (!v)
            return NULL;
        return ast_make_assign(expr->assign.name, v,
                               expr->line, expr->column);
    }
    case EXPR_INDEX: {
        expr_t *a = clone_expr(expr->index.array);
        expr_t *i = clone_expr(expr->index.index);
        if (!a || !i) {
            ast_free_expr(a);
            ast_free_expr(i);
            return NULL;
        }
        return ast_make_index(a, i, expr->line, expr->column);
    }
    case EXPR_ASSIGN_INDEX: {
        expr_t *a = clone_expr(expr->assign_index.array);
        expr_t *i = clone_expr(expr->assign_index.index);
        expr_t *v = clone_expr(expr->assign_index.value);
        if (!a || !i || !v) {
            ast_free_expr(a);
            ast_free_expr(i);
            ast_free_expr(v);
            return NULL;
        }
        return ast_make_assign_index(a, i, v,
                                     expr->line, expr->column);
    }
    case EXPR_ASSIGN_MEMBER: {
        expr_t *obj = clone_expr(expr->assign_member.object);
        expr_t *val = clone_expr(expr->assign_member.value);
        if (!obj || !val) {
            ast_free_expr(obj);
            ast_free_expr(val);
            return NULL;
        }
        return ast_make_assign_member(obj, expr->assign_member.member,
                                      val, expr->assign_member.via_ptr,
                                      expr->line, expr->column);
    }
    case EXPR_MEMBER: {
        expr_t *obj = clone_expr(expr->member.object);
        if (!obj)
            return NULL;
        return ast_make_member(obj, expr->member.member,
                               expr->member.via_ptr,
                               expr->line, expr->column);
    }
    case EXPR_SIZEOF:
        if (expr->sizeof_expr.is_type)
            return ast_make_sizeof_type(expr->sizeof_expr.type,
                                        expr->sizeof_expr.array_size,
                                        expr->sizeof_expr.elem_size,
                                        expr->line, expr->column);
        else {
            expr_t *e = clone_expr(expr->sizeof_expr.expr);
            if (!e)
                return NULL;
            return ast_make_sizeof_expr(e, expr->line, expr->column);
        }
    case EXPR_CALL: {
        size_t n = expr->call.arg_count;
        expr_t **args = NULL;
        if (n) {
            args = malloc(n * sizeof(*args));
            if (!args)
                return NULL;
            for (size_t i = 0; i < n; i++) {
                args[i] = clone_expr(expr->call.args[i]);
                if (!args[i]) {
                    for (size_t j = 0; j < i; j++)
                        ast_free_expr(args[j]);
                    free(args);
                    return NULL;
                }
            }
        }
        return ast_make_call(expr->call.name, args, n,
                             expr->line, expr->column);
    }
    case EXPR_COMPLIT: {
        expr_t *init = clone_expr(expr->compound.init);
        init_entry_t *list = NULL;
        if (expr->compound.init_count) {
            list = malloc(expr->compound.init_count * sizeof(*list));
            if (!list) {
                ast_free_expr(init);
                return NULL;
            }
            for (size_t i = 0; i < expr->compound.init_count; i++) {
                list[i].kind = expr->compound.init_list[i].kind;
                list[i].field = expr->compound.init_list[i].field ? vc_strdup(expr->compound.init_list[i].field) : NULL;
                list[i].index = clone_expr(expr->compound.init_list[i].index);
                list[i].value = clone_expr(expr->compound.init_list[i].value);
                if ((expr->compound.init_list[i].field && !list[i].field) ||
                    (expr->compound.init_list[i].index && !list[i].index) ||
                    (expr->compound.init_list[i].value && !list[i].value)) {
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
        return ast_make_compound(expr->compound.type, expr->compound.array_size,
                                 expr->compound.elem_size, init, list,
                                 expr->compound.init_count, expr->line,
                                 expr->column);
    }
    }
    return NULL;
}

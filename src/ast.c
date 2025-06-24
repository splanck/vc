#include <stdlib.h>
#include <string.h>
#include "ast.h"

static char *dup_string(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len + 1);
    return out;
}
/* Constructors for expressions */
expr_t *ast_make_number(const char *value)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_NUMBER;
    expr->number.value = dup_string(value ? value : "");
    if (!expr->number.value) {
        free(expr);
        return NULL;
    }
    return expr;
}

expr_t *ast_make_ident(const char *name)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_IDENT;
    expr->ident.name = dup_string(name ? name : "");
    if (!expr->ident.name) {
        free(expr);
        return NULL;
    }
    return expr;
}

expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_BINARY;
    expr->binary.op = op;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

expr_t *ast_make_assign(const char *name, expr_t *value)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_ASSIGN;
    expr->assign.name = dup_string(name ? name : "");
    if (!expr->assign.name) {
        free(expr);
        return NULL;
    }
    expr->assign.value = value;
    return expr;
}

expr_t *ast_make_call(const char *name)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_CALL;
    expr->call.name = dup_string(name ? name : "");
    if (!expr->call.name) {
        free(expr);
        return NULL;
    }
    return expr;
}

/* Constructors for statements */
stmt_t *ast_make_expr_stmt(expr_t *expr)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_EXPR;
    stmt->expr.expr = expr;
    return stmt;
}

stmt_t *ast_make_return(expr_t *expr)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_RETURN;
    stmt->ret.expr = expr;
    return stmt;
}

stmt_t *ast_make_var_decl(const char *name)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_VAR_DECL;
    stmt->var_decl.name = dup_string(name ? name : "");
    if (!stmt->var_decl.name) {
        free(stmt);
        return NULL;
    }
    return stmt;
}

func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      stmt_t **body, size_t body_count)
{
    func_t *fn = malloc(sizeof(*fn));
    if (!fn)
        return NULL;
    fn->name = dup_string(name ? name : "");
    if (!fn->name) {
        free(fn);
        return NULL;
    }
    fn->return_type = ret_type;
    fn->body = body;
    fn->body_count = body_count;
    return fn;
}

/* Destructors */
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
    case EXPR_BINARY:
        ast_free_expr(expr->binary.left);
        ast_free_expr(expr->binary.right);
        break;
    case EXPR_ASSIGN:
        free(expr->assign.name);
        ast_free_expr(expr->assign.value);
        break;
    case EXPR_CALL:
        free(expr->call.name);
        break;
    }
    free(expr);
}

void ast_free_stmt(stmt_t *stmt)
{
    if (!stmt)
        return;
    switch (stmt->kind) {
    case STMT_EXPR:
        ast_free_expr(stmt->expr.expr);
        break;
    case STMT_RETURN:
        ast_free_expr(stmt->ret.expr);
        break;
    case STMT_VAR_DECL:
        free(stmt->var_decl.name);
        break;
    }
    free(stmt);
}

void ast_free_func(func_t *func)
{
    if (!func)
        return;
    for (size_t i = 0; i < func->body_count; i++)
        ast_free_stmt(func->body[i]);
    free(func->body);
    free(func->name);
    free(func);
}


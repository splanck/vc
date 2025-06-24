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
expr_t *ast_make_number(const char *value, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_NUMBER;
    expr->line = line;
    expr->column = column;
    expr->number.value = dup_string(value ? value : "");
    if (!expr->number.value) {
        free(expr);
        return NULL;
    }
    return expr;
}

expr_t *ast_make_ident(const char *name, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_IDENT;
    expr->line = line;
    expr->column = column;
    expr->ident.name = dup_string(name ? name : "");
    if (!expr->ident.name) {
        free(expr);
        return NULL;
    }
    return expr;
}

expr_t *ast_make_string(const char *value, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_STRING;
    expr->line = line;
    expr->column = column;
    expr->string.value = dup_string(value ? value : "");
    if (!expr->string.value) {
        free(expr);
        return NULL;
    }
    return expr;
}

expr_t *ast_make_char(char value, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_CHAR;
    expr->line = line;
    expr->column = column;
    expr->ch.value = value;
    return expr;
}

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

expr_t *ast_make_assign(const char *name, expr_t *value,
                        size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_ASSIGN;
    expr->line = line;
    expr->column = column;
    expr->assign.name = dup_string(name ? name : "");
    if (!expr->assign.name) {
        free(expr);
        return NULL;
    }
    expr->assign.value = value;
    return expr;
}

expr_t *ast_make_call(const char *name, expr_t **args, size_t arg_count,
                      size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_CALL;
    expr->line = line;
    expr->column = column;
    expr->call.name = dup_string(name ? name : "");
    if (!expr->call.name) {
        free(expr);
        return NULL;
    }
    expr->call.args = args;
    expr->call.arg_count = arg_count;
    return expr;
}

/* Constructors for statements */
stmt_t *ast_make_expr_stmt(expr_t *expr, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_EXPR;
    stmt->line = line;
    stmt->column = column;
    stmt->expr.expr = expr;
    return stmt;
}

stmt_t *ast_make_return(expr_t *expr, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_RETURN;
    stmt->line = line;
    stmt->column = column;
    stmt->ret.expr = expr;
    return stmt;
}

stmt_t *ast_make_var_decl(const char *name, type_kind_t type, expr_t *init,
                          size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_VAR_DECL;
    stmt->line = line;
    stmt->column = column;
    stmt->var_decl.name = dup_string(name ? name : "");
    if (!stmt->var_decl.name) {
        free(stmt);
        return NULL;
    }
    stmt->var_decl.type = type;
    stmt->var_decl.init = init;
    return stmt;
}

stmt_t *ast_make_if(expr_t *cond, stmt_t *then_branch, stmt_t *else_branch,
                    size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_IF;
    stmt->line = line;
    stmt->column = column;
    stmt->if_stmt.cond = cond;
    stmt->if_stmt.then_branch = then_branch;
    stmt->if_stmt.else_branch = else_branch;
    return stmt;
}

stmt_t *ast_make_while(expr_t *cond, stmt_t *body,
                       size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_WHILE;
    stmt->line = line;
    stmt->column = column;
    stmt->while_stmt.cond = cond;
    stmt->while_stmt.body = body;
    return stmt;
}

stmt_t *ast_make_for(expr_t *init, expr_t *cond, expr_t *incr, stmt_t *body,
                     size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_FOR;
    stmt->line = line;
    stmt->column = column;
    stmt->for_stmt.init = init;
    stmt->for_stmt.cond = cond;
    stmt->for_stmt.incr = incr;
    stmt->for_stmt.body = body;
    return stmt;
}

stmt_t *ast_make_break(size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_BREAK;
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

stmt_t *ast_make_continue(size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_CONTINUE;
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

stmt_t *ast_make_block(stmt_t **stmts, size_t count,
                       size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_BLOCK;
    stmt->line = line;
    stmt->column = column;
    stmt->block.stmts = stmts;
    stmt->block.count = count;
    return stmt;
}

func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      char **param_names, type_kind_t *param_types,
                      size_t param_count,
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
    fn->param_count = param_count;
    fn->param_names = malloc(param_count * sizeof(*fn->param_names));
    fn->param_types = malloc(param_count * sizeof(*fn->param_types));
    if ((param_count && (!fn->param_names || !fn->param_types))) {
        free(fn->name);
        free(fn->param_names);
        free(fn->param_types);
        free(fn);
        return NULL;
    }
    for (size_t i = 0; i < param_count; i++) {
        fn->param_names[i] = dup_string(param_names[i] ? param_names[i] : "");
        fn->param_types[i] = param_types[i];
        if (!fn->param_names[i]) {
            for (size_t j = 0; j < i; j++)
                free(fn->param_names[j]);
            free(fn->param_names);
            free(fn->param_types);
            free(fn->name);
            free(fn);
            return NULL;
        }
    }
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
    case EXPR_ASSIGN:
        free(expr->assign.name);
        ast_free_expr(expr->assign.value);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < expr->call.arg_count; i++)
            ast_free_expr(expr->call.args[i]);
        free(expr->call.args);
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
        ast_free_expr(stmt->var_decl.init);
        break;
    case STMT_IF:
        ast_free_expr(stmt->if_stmt.cond);
        ast_free_stmt(stmt->if_stmt.then_branch);
        ast_free_stmt(stmt->if_stmt.else_branch);
        break;
    case STMT_WHILE:
        ast_free_expr(stmt->while_stmt.cond);
        ast_free_stmt(stmt->while_stmt.body);
        break;
    case STMT_FOR:
        ast_free_expr(stmt->for_stmt.init);
        ast_free_expr(stmt->for_stmt.cond);
        ast_free_expr(stmt->for_stmt.incr);
        ast_free_stmt(stmt->for_stmt.body);
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
        break;
    case STMT_BLOCK:
        for (size_t i = 0; i < stmt->block.count; i++)
            ast_free_stmt(stmt->block.stmts[i]);
        free(stmt->block.stmts);
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
    for (size_t i = 0; i < func->param_count; i++)
        free(func->param_names[i]);
    free(func->param_names);
    free(func->param_types);
    free(func->name);
    free(func);
}


/*
 * AST constructors and helpers for the compiler.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ast.h"
#include "util.h"
/* Constructors for expressions */
/* Create a numeric literal expression node. */
expr_t *ast_make_number(const char *value, size_t line, size_t column)
{
    expr_t *expr = malloc(sizeof(*expr));
    if (!expr)
        return NULL;
    expr->kind = EXPR_NUMBER;
    expr->line = line;
    expr->column = column;
    expr->number.value = vc_strdup(value ? value : "");
    if (!expr->number.value) {
        free(expr);
        return NULL;
    }
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
expr_t *ast_make_string(const char *value, size_t line, size_t column)
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
    return expr;
}

/* Create a character literal expression node. */
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
                             size_t line, size_t column)
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
    expr->sizeof_expr.expr = e;
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

/* Constructors for statements */
/* Wrap an expression as a statement. */
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

/* Create a return statement node. */
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

/* Create a variable declaration statement. */
stmt_t *ast_make_var_decl(const char *name, type_kind_t type, size_t array_size,
                          expr_t *init, expr_t **init_list, size_t init_count,
                          size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_VAR_DECL;
    stmt->line = line;
    stmt->column = column;
    stmt->var_decl.name = vc_strdup(name ? name : "");
    if (!stmt->var_decl.name) {
        free(stmt);
        return NULL;
    }
    stmt->var_decl.type = type;
    stmt->var_decl.array_size = array_size;
    stmt->var_decl.init = init;
    stmt->var_decl.init_list = init_list;
    stmt->var_decl.init_count = init_count;
    return stmt;
}

/* Create an if/else statement node. */
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

/* Create a while loop statement node. */
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

/* Create a do-while loop statement node. */
stmt_t *ast_make_do_while(expr_t *cond, stmt_t *body,
                          size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_DO_WHILE;
    stmt->line = line;
    stmt->column = column;
    stmt->do_while_stmt.cond = cond;
    stmt->do_while_stmt.body = body;
    return stmt;
}

/* Create a for loop statement node. */
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

/* Create a switch statement node. */
stmt_t *ast_make_switch(expr_t *expr, switch_case_t *cases, size_t case_count,
                        stmt_t *default_body, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_SWITCH;
    stmt->line = line;
    stmt->column = column;
    stmt->switch_stmt.expr = expr;
    stmt->switch_stmt.cases = cases;
    stmt->switch_stmt.case_count = case_count;
    stmt->switch_stmt.default_body = default_body;
    return stmt;
}

/* Create a break statement node. */
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

/* Create a continue statement node. */
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

/* Create a label statement */
stmt_t *ast_make_label(const char *name, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_LABEL;
    stmt->line = line;
    stmt->column = column;
    stmt->label.name = vc_strdup(name ? name : "");
    if (!stmt->label.name) {
        free(stmt);
        return NULL;
    }
    return stmt;
}

/* Create a goto statement */
stmt_t *ast_make_goto(const char *name, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_GOTO;
    stmt->line = line;
    stmt->column = column;
    stmt->goto_stmt.name = vc_strdup(name ? name : "");
    if (!stmt->goto_stmt.name) {
        free(stmt);
        return NULL;
    }
    return stmt;
}

/* Create a typedef declaration */
stmt_t *ast_make_typedef(const char *name, type_kind_t type, size_t array_size,
                         size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_TYPEDEF;
    stmt->line = line;
    stmt->column = column;
    stmt->typedef_decl.name = vc_strdup(name ? name : "");
    if (!stmt->typedef_decl.name) {
        free(stmt);
        return NULL;
    }
    stmt->typedef_decl.type = type;
    stmt->typedef_decl.array_size = array_size;
    return stmt;
}

/* Create an enum declaration statement */
stmt_t *ast_make_enum_decl(const char *tag, enumerator_t *items, size_t count,
                           size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_ENUM_DECL;
    stmt->line = line;
    stmt->column = column;
    stmt->enum_decl.tag = vc_strdup(tag ? tag : "");
    if (!stmt->enum_decl.tag) {
        free(stmt);
        return NULL;
    }
    stmt->enum_decl.items = items;
    stmt->enum_decl.count = count;
    return stmt;
}

/* Create a block statement containing \p count child statements. */
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

/* Create a function definition node with parameters and body. */
func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      char **param_names, type_kind_t *param_types,
                      size_t param_count,
                      stmt_t **body, size_t body_count)
{
    func_t *fn = malloc(sizeof(*fn));
    if (!fn)
        return NULL;
    fn->name = vc_strdup(name ? name : "");
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
        fn->param_names[i] = vc_strdup(param_names[i] ? param_names[i] : "");
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
    case EXPR_MEMBER:
        ast_free_expr(expr->member.object);
        free(expr->member.member);
        break;
    case EXPR_SIZEOF:
        if (!expr->sizeof_expr.is_type)
            ast_free_expr(expr->sizeof_expr.expr);
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

/* Free a statement node and all of its children. */
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
        for (size_t i = 0; i < stmt->var_decl.init_count; i++)
            ast_free_expr(stmt->var_decl.init_list[i]);
        free(stmt->var_decl.init_list);
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
    case STMT_DO_WHILE:
        ast_free_expr(stmt->do_while_stmt.cond);
        ast_free_stmt(stmt->do_while_stmt.body);
        break;
    case STMT_FOR:
        ast_free_expr(stmt->for_stmt.init);
        ast_free_expr(stmt->for_stmt.cond);
        ast_free_expr(stmt->for_stmt.incr);
        ast_free_stmt(stmt->for_stmt.body);
        break;
    case STMT_SWITCH:
        ast_free_expr(stmt->switch_stmt.expr);
        for (size_t i = 0; i < stmt->switch_stmt.case_count; i++) {
            ast_free_expr(stmt->switch_stmt.cases[i].expr);
            ast_free_stmt(stmt->switch_stmt.cases[i].body);
        }
        free(stmt->switch_stmt.cases);
        ast_free_stmt(stmt->switch_stmt.default_body);
        break;
    case STMT_LABEL:
        free(stmt->label.name);
        break;
    case STMT_GOTO:
        free(stmt->goto_stmt.name);
        break;
    case STMT_TYPEDEF:
        free(stmt->typedef_decl.name);
        break;
    case STMT_ENUM_DECL:
        free(stmt->enum_decl.tag);
        for (size_t i = 0; i < stmt->enum_decl.count; i++) {
            free(stmt->enum_decl.items[i].name);
            ast_free_expr(stmt->enum_decl.items[i].value);
        }
        free(stmt->enum_decl.items);
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

/* Free a function definition and its entire body. */
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


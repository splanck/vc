/*
 * AST statement constructors for the compiler.
 *
 * This file implements the `ast_make_*` helpers declared in `ast_stmt.h`.
 * Each routine allocates a new node that forms part of the abstract
 * syntax tree.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ast_stmt.h"
#include "ast_expr.h"
#include "util.h"

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
                          expr_t *size_expr, expr_t *align_expr,
                          size_t elem_size, int is_static, int is_register,
                          int is_extern, int is_const, int is_volatile, int is_restrict,
                          expr_t *init, init_entry_t *init_list, size_t init_count,
                          const char *tag, union_member_t *members,
                          size_t member_count, size_t line, size_t column)
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
    stmt->var_decl.size_expr = size_expr;
    stmt->var_decl.align_expr = align_expr;
    stmt->var_decl.alignment = 0;
    stmt->var_decl.elem_size = elem_size;
    if (tag) {
        stmt->var_decl.tag = vc_strdup(tag);
        if (!stmt->var_decl.tag) {
            free(stmt->var_decl.name);
            free(stmt);
            return NULL;
        }
    } else {
        stmt->var_decl.tag = NULL;
    }
    stmt->var_decl.is_static = is_static;
    stmt->var_decl.is_register = is_register;
    stmt->var_decl.is_extern = is_extern;
    stmt->var_decl.is_const = is_const;
    stmt->var_decl.is_volatile = is_volatile;
    stmt->var_decl.is_restrict = is_restrict;
    stmt->var_decl.init = init;
    stmt->var_decl.init_list = init_list;
    stmt->var_decl.init_count = init_count;
    stmt->var_decl.members = members;
    stmt->var_decl.member_count = member_count;
    stmt->var_decl.func_ret_type = TYPE_UNKNOWN;
    stmt->var_decl.func_param_types = NULL;
    stmt->var_decl.func_param_count = 0;
    stmt->var_decl.func_variadic = 0;
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
stmt_t *ast_make_for(stmt_t *init_decl, expr_t *init, expr_t *cond,
                     expr_t *incr, stmt_t *body,
                     size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_FOR;
    stmt->line = line;
    stmt->column = column;
    stmt->for_stmt.init_decl = init_decl;
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

/* Create a _Static_assert statement */
stmt_t *ast_make_static_assert(expr_t *expr, const char *msg,
                               size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_STATIC_ASSERT;
    stmt->line = line;
    stmt->column = column;
    stmt->static_assert.expr = expr;
    stmt->static_assert.message = vc_strdup(msg ? msg : "");
    if (!stmt->static_assert.message) {
        free(stmt);
        return NULL;
    }
    return stmt;
}

/* Create a typedef declaration */
stmt_t *ast_make_typedef(const char *name, type_kind_t type, size_t array_size,
                         size_t elem_size, size_t line, size_t column)
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
    stmt->typedef_decl.elem_size = elem_size;
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

/* Create a struct declaration statement */
stmt_t *ast_make_struct_decl(const char *tag, struct_member_t *members,
                             size_t count, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_STRUCT_DECL;
    stmt->line = line;
    stmt->column = column;
    stmt->struct_decl.tag = vc_strdup(tag ? tag : "");
    if (!stmt->struct_decl.tag) {
        free(stmt);
        return NULL;
    }
    stmt->struct_decl.members = members;
    stmt->struct_decl.count = count;
    return stmt;
}

/* Create a union declaration statement */
stmt_t *ast_make_union_decl(const char *tag, union_member_t *members,
                            size_t count, size_t line, size_t column)
{
    stmt_t *stmt = malloc(sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->kind = STMT_UNION_DECL;
    stmt->line = line;
    stmt->column = column;
    stmt->union_decl.tag = vc_strdup(tag ? tag : "");
    if (!stmt->union_decl.tag) {
        free(stmt);
        return NULL;
    }
    stmt->union_decl.members = members;
    stmt->union_decl.count = count;
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
                      const char *ret_tag,
                      char **param_names, type_kind_t *param_types,
                      char **param_tags,
                      size_t *param_elem_sizes, int *param_is_restrict,
                      size_t param_count, int is_variadic,
                      stmt_t **body, size_t body_count,
                      int is_inline, int is_noreturn)
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
    fn->return_tag = vc_strdup(ret_tag ? ret_tag : "");
    fn->param_count = param_count;
    fn->is_variadic = is_variadic;
    fn->param_names = malloc(param_count * sizeof(*fn->param_names));
    fn->param_types = malloc(param_count * sizeof(*fn->param_types));
    fn->param_tags = malloc(param_count * sizeof(*fn->param_tags));
    fn->param_elem_sizes = malloc(param_count * sizeof(*fn->param_elem_sizes));
    fn->param_is_restrict = malloc(param_count * sizeof(*fn->param_is_restrict));
    if ((param_count && (!fn->param_names || !fn->param_types || !fn->param_tags || !fn->param_elem_sizes || !fn->param_is_restrict)) || !fn->return_tag) {
        free(fn->name);
        free(fn->param_names);
        free(fn->param_types);
        free(fn->param_tags);
        free(fn->param_elem_sizes);
        free(fn->param_is_restrict);
        free(fn->return_tag);
        free(fn);
        return NULL;
    }
    for (size_t i = 0; i < param_count; i++) {
        fn->param_names[i] = vc_strdup(param_names[i] ? param_names[i] : "");
        fn->param_types[i] = param_types[i];
        fn->param_tags[i] = vc_strdup(param_tags && param_tags[i] ? param_tags[i] : "");
        fn->param_elem_sizes[i] = param_elem_sizes ? param_elem_sizes[i] : 4;
        fn->param_is_restrict[i] = param_is_restrict ? param_is_restrict[i] : 0;
        if (!fn->param_names[i] || !fn->param_tags[i]) {
            for (size_t j = 0; j < i; j++)
                free(fn->param_names[j]);
            for (size_t j = 0; j < i; j++)
                free(fn->param_tags[j]);
            free(fn->param_names);
            free(fn->param_types);
            free(fn->param_tags);
            free(fn->param_elem_sizes);
            free(fn->param_is_restrict);
            free(fn->return_tag);
            free(fn->name);
            free(fn);
            return NULL;
        }
    }
    fn->body = body;
    fn->body_count = body_count;
    fn->is_inline = is_inline;
    fn->is_noreturn = is_noreturn;
    return fn;
}

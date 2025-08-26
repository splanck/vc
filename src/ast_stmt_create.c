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

/* Internal helper for variable declarations */
int init_var_decl(stmt_t *stmt, const char *name, const char *tag)
{
    STMT_VAR_DECL(stmt).name = vc_strdup(name ? name : "");
    if (!STMT_VAR_DECL(stmt).name)
        return 0;
    if (tag) {
        STMT_VAR_DECL(stmt).tag = vc_strdup(tag);
        if (!STMT_VAR_DECL(stmt).tag) {
            free(STMT_VAR_DECL(stmt).name);
            return 0;
        }
    } else {
        STMT_VAR_DECL(stmt).tag = NULL;
    }
    return 1;
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
    STMT_EXPR(stmt).expr = expr;
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
    STMT_RET(stmt).expr = expr;
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
    if (!init_var_decl(stmt, name, tag)) {
        free(stmt);
        return NULL;
    }
    STMT_VAR_DECL(stmt).type = type;
    STMT_VAR_DECL(stmt).array_size = array_size;
    STMT_VAR_DECL(stmt).size_expr = size_expr;
    STMT_VAR_DECL(stmt).align_expr = align_expr;
    STMT_VAR_DECL(stmt).alignment = 0;
    STMT_VAR_DECL(stmt).elem_size = elem_size;
    STMT_VAR_DECL(stmt).is_static = is_static;
    STMT_VAR_DECL(stmt).is_register = is_register;
    STMT_VAR_DECL(stmt).is_extern = is_extern;
    STMT_VAR_DECL(stmt).is_const = is_const;
    STMT_VAR_DECL(stmt).is_volatile = is_volatile;
    STMT_VAR_DECL(stmt).is_restrict = is_restrict;
    STMT_VAR_DECL(stmt).init = init;
    STMT_VAR_DECL(stmt).init_list = init_list;
    STMT_VAR_DECL(stmt).init_count = init_count;
    STMT_VAR_DECL(stmt).members = members;
    STMT_VAR_DECL(stmt).member_count = member_count;
    STMT_VAR_DECL(stmt).func_ret_type = TYPE_UNKNOWN;
    STMT_VAR_DECL(stmt).func_param_types = NULL;
    STMT_VAR_DECL(stmt).func_param_count = 0;
    STMT_VAR_DECL(stmt).func_variadic = 0;
    STMT_VAR_DECL(stmt).next = NULL;
    STMT_VAR_DECL(stmt).next_count = 0;
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
    STMT_IF(stmt).cond = cond;
    STMT_IF(stmt).then_branch = then_branch;
    STMT_IF(stmt).else_branch = else_branch;
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
    STMT_WHILE(stmt).cond = cond;
    STMT_WHILE(stmt).body = body;
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
    STMT_DO_WHILE(stmt).cond = cond;
    STMT_DO_WHILE(stmt).body = body;
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
    STMT_FOR(stmt).init_decl = init_decl;
    STMT_FOR(stmt).init = init;
    STMT_FOR(stmt).cond = cond;
    STMT_FOR(stmt).incr = incr;
    STMT_FOR(stmt).body = body;
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
    STMT_SWITCH(stmt).expr = expr;
    STMT_SWITCH(stmt).cases = cases;
    STMT_SWITCH(stmt).case_count = case_count;
    STMT_SWITCH(stmt).default_body = default_body;
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
    STMT_LABEL(stmt).name = vc_strdup(name ? name : "");
    if (!STMT_LABEL(stmt).name) {
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
    STMT_GOTO(stmt).name = vc_strdup(name ? name : "");
    if (!STMT_GOTO(stmt).name) {
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
    STMT_STATIC_ASSERT(stmt).expr = expr;
    STMT_STATIC_ASSERT(stmt).message = vc_strdup(msg ? msg : "");
    if (!STMT_STATIC_ASSERT(stmt).message) {
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
    STMT_TYPEDEF(stmt).name = vc_strdup(name ? name : "");
    if (!STMT_TYPEDEF(stmt).name) {
        free(stmt);
        return NULL;
    }
    STMT_TYPEDEF(stmt).type = type;
    STMT_TYPEDEF(stmt).array_size = array_size;
    STMT_TYPEDEF(stmt).elem_size = elem_size;
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
    STMT_ENUM_DECL(stmt).tag = vc_strdup(tag ? tag : "");
    if (!STMT_ENUM_DECL(stmt).tag) {
        free(stmt);
        return NULL;
    }
    STMT_ENUM_DECL(stmt).items = items;
    STMT_ENUM_DECL(stmt).count = count;
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
    STMT_STRUCT_DECL(stmt).tag = vc_strdup(tag ? tag : "");
    if (!STMT_STRUCT_DECL(stmt).tag) {
        free(stmt);
        return NULL;
    }
    STMT_STRUCT_DECL(stmt).members = members;
    STMT_STRUCT_DECL(stmt).count = count;
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
    STMT_UNION_DECL(stmt).tag = vc_strdup(tag ? tag : "");
    if (!STMT_UNION_DECL(stmt).tag) {
        free(stmt);
        return NULL;
    }
    STMT_UNION_DECL(stmt).members = members;
    STMT_UNION_DECL(stmt).count = count;
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
    STMT_BLOCK(stmt).stmts = stmts;
    STMT_BLOCK(stmt).count = count;
    return stmt;
}

/* Allocate function parameter arrays for \p fn. */
static int alloc_func_params(func_t *fn, size_t count)
{
    fn->param_names = malloc(count * sizeof(*fn->param_names));
    fn->param_types = malloc(count * sizeof(*fn->param_types));
    fn->param_tags = malloc(count * sizeof(*fn->param_tags));
    fn->param_elem_sizes = malloc(count * sizeof(*fn->param_elem_sizes));
    fn->param_is_restrict = malloc(count * sizeof(*fn->param_is_restrict));

    if (count && (!fn->param_names || !fn->param_types || !fn->param_tags ||
                  !fn->param_elem_sizes || !fn->param_is_restrict)) {
        free(fn->param_names);
        free(fn->param_types);
        free(fn->param_tags);
        free(fn->param_elem_sizes);
        free(fn->param_is_restrict);
        return -1;
    }
    return 0;
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
    if (!fn->return_tag || alloc_func_params(fn, param_count)) {
        free(fn->name);
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

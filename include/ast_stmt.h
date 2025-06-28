/*
 * Statement and function AST construction helpers.
 *
 * These routines mirror those in ``ast_expr.h'' but operate on the various
 * statement node types and complete function definitions.  Each constructor
 * allocates a new node and returns it to the caller, who is then
 * responsible for eventually freeing the structure with ``ast_free_stmt'' or
 * ``ast_free_func''.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_AST_STMT_H
#define VC_AST_STMT_H

#include "ast.h"

/* Create a statement from a single expression. */
stmt_t *ast_make_expr_stmt(expr_t *expr, size_t line, size_t column);
/* Create a return statement. */
stmt_t *ast_make_return(expr_t *expr, size_t line, size_t column);
/* Create a variable declaration statement. */
stmt_t *ast_make_var_decl(const char *name, type_kind_t type, size_t array_size,
                          expr_t *size_expr, size_t elem_size, int is_static,
                          int is_register, int is_extern, int is_const,
                          int is_volatile, int is_restrict,
                          expr_t *init, init_entry_t *init_list, size_t init_count,
                          const char *tag, union_member_t *members,
                          size_t member_count, size_t line, size_t column);
/* Create an if statement. */
stmt_t *ast_make_if(expr_t *cond, stmt_t *then_branch, stmt_t *else_branch,
                    size_t line, size_t column);
/* Create a while loop. */
stmt_t *ast_make_while(expr_t *cond, stmt_t *body,
                       size_t line, size_t column);
/* Create a do-while loop. */
stmt_t *ast_make_do_while(expr_t *cond, stmt_t *body,
                          size_t line, size_t column);
/* Create a for loop. */
stmt_t *ast_make_for(stmt_t *init_decl, expr_t *init, expr_t *cond,
                     expr_t *incr, stmt_t *body,
                     size_t line, size_t column);
/* Create a switch statement. */
stmt_t *ast_make_switch(expr_t *expr, switch_case_t *cases, size_t case_count,
                        stmt_t *default_body, size_t line, size_t column);
/* Create a break statement. */
stmt_t *ast_make_break(size_t line, size_t column);
/* Create a continue statement. */
stmt_t *ast_make_continue(size_t line, size_t column);
/* Create a label statement. */
stmt_t *ast_make_label(const char *name, size_t line, size_t column);
/* Create a goto statement. */
stmt_t *ast_make_goto(const char *name, size_t line, size_t column);
/* Create a typedef declaration. */
stmt_t *ast_make_typedef(const char *name, type_kind_t type, size_t array_size,
                         size_t elem_size, size_t line, size_t column);
/* Create an enum declaration. */
stmt_t *ast_make_enum_decl(const char *tag, enumerator_t *items, size_t count,
                           size_t line, size_t column);
/* Create a struct declaration. */
stmt_t *ast_make_struct_decl(const char *tag, struct_member_t *members,
                             size_t count, size_t line, size_t column);
/* Create a union declaration. */
stmt_t *ast_make_union_decl(const char *tag, union_member_t *members,
                            size_t count, size_t line, size_t column);
/* Create a block containing an array of statements. */
stmt_t *ast_make_block(stmt_t **stmts, size_t count,
                       size_t line, size_t column);

/* Create a function definition. */
func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      char **param_names, type_kind_t *param_types,
                      size_t *param_elem_sizes, int *param_is_restrict,
                      size_t param_count, int is_variadic,
                      stmt_t **body, size_t body_count,
                      int is_inline);

/* Recursively free a statement tree. */
void ast_free_stmt(stmt_t *stmt);
/* Free a function definition. */
void ast_free_func(func_t *func);

#endif /* VC_AST_STMT_H */

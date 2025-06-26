#ifndef VC_AST_STMT_H
#define VC_AST_STMT_H

#include "ast.h"

stmt_t *ast_make_expr_stmt(expr_t *expr, size_t line, size_t column);
stmt_t *ast_make_return(expr_t *expr, size_t line, size_t column);
stmt_t *ast_make_var_decl(const char *name, type_kind_t type, size_t array_size,
                          expr_t *size_expr, size_t elem_size, int is_static,
                          int is_register, int is_extern, int is_const,
                          int is_volatile, int is_restrict,
                          expr_t *init, init_entry_t *init_list, size_t init_count,
                          const char *tag, union_member_t *members,
                          size_t member_count, size_t line, size_t column);
stmt_t *ast_make_if(expr_t *cond, stmt_t *then_branch, stmt_t *else_branch,
                    size_t line, size_t column);
stmt_t *ast_make_while(expr_t *cond, stmt_t *body,
                       size_t line, size_t column);
stmt_t *ast_make_do_while(expr_t *cond, stmt_t *body,
                          size_t line, size_t column);
stmt_t *ast_make_for(stmt_t *init_decl, expr_t *init, expr_t *cond,
                     expr_t *incr, stmt_t *body,
                     size_t line, size_t column);
stmt_t *ast_make_switch(expr_t *expr, switch_case_t *cases, size_t case_count,
                        stmt_t *default_body, size_t line, size_t column);
stmt_t *ast_make_break(size_t line, size_t column);
stmt_t *ast_make_continue(size_t line, size_t column);
stmt_t *ast_make_label(const char *name, size_t line, size_t column);
stmt_t *ast_make_goto(const char *name, size_t line, size_t column);
stmt_t *ast_make_typedef(const char *name, type_kind_t type, size_t array_size,
                         size_t elem_size, size_t line, size_t column);
stmt_t *ast_make_enum_decl(const char *tag, enumerator_t *items, size_t count,
                           size_t line, size_t column);
stmt_t *ast_make_struct_decl(const char *tag, struct_member_t *members,
                             size_t count, size_t line, size_t column);
stmt_t *ast_make_union_decl(const char *tag, union_member_t *members,
                            size_t count, size_t line, size_t column);
stmt_t *ast_make_block(stmt_t **stmts, size_t count,
                       size_t line, size_t column);

func_t *ast_make_func(const char *name, type_kind_t ret_type,
                      char **param_names, type_kind_t *param_types,
                      size_t *param_elem_sizes, int *param_is_restrict,
                      size_t param_count,
                      stmt_t **body, size_t body_count);

void ast_free_stmt(stmt_t *stmt);
void ast_free_func(func_t *func);

#endif /* VC_AST_STMT_H */

/*
 * Expression AST construction helpers.
 *
 * These routines allocate and initialise the expression nodes defined in
 * ``ast.h''.  Each constructor returns a pointer to a newly allocated node
 * or ``NULL'' on failure.  The caller becomes responsible for freeing the
 * returned tree using ``ast_free_expr''.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_AST_EXPR_H
#define VC_AST_EXPR_H

#include "ast.h"

/* Create a numeric literal expression. */
expr_t *ast_make_number(const char *value, size_t line, size_t column);
/* Create an identifier expression. */
expr_t *ast_make_ident(const char *name, size_t line, size_t column);
/* Create a string literal expression. */
expr_t *ast_make_string(const char *value, size_t line, size_t column);
/* Create a wide string literal expression. */
expr_t *ast_make_wstring(const char *value, size_t line, size_t column);
/* Create a character literal expression. */
expr_t *ast_make_char(char value, size_t line, size_t column);
/* Create a wide character literal expression. */
expr_t *ast_make_wchar(char value, size_t line, size_t column);
/* Create a binary operation expression. */
expr_t *ast_make_binary(binop_t op, expr_t *left, expr_t *right,
                        size_t line, size_t column);
/* Create a unary operation expression. */
expr_t *ast_make_unary(unop_t op, expr_t *operand,
                       size_t line, size_t column);
/* Create a conditional expression. */
expr_t *ast_make_cond(expr_t *cond, expr_t *then_expr, expr_t *else_expr,
                      size_t line, size_t column);
/* Create an assignment to a variable. */
expr_t *ast_make_assign(const char *name, expr_t *value,
                        size_t line, size_t column);
/* Create an array indexing expression. */
expr_t *ast_make_index(expr_t *array, expr_t *index,
                       size_t line, size_t column);
/* Create an assignment to an array element. */
expr_t *ast_make_assign_index(expr_t *array, expr_t *index, expr_t *value,
                              size_t line, size_t column);
/* Create an assignment to a struct or union member. */
expr_t *ast_make_assign_member(expr_t *object, const char *member, expr_t *value,
                               int via_ptr, size_t line, size_t column);
/* Create a member access expression. */
expr_t *ast_make_member(expr_t *object, const char *member, int via_ptr,
                        size_t line, size_t column);
/* Create a sizeof expression for a type. */
expr_t *ast_make_sizeof_type(type_kind_t type, size_t array_size,
                             size_t elem_size, size_t line, size_t column);
/* Create a sizeof expression for another expression. */
expr_t *ast_make_sizeof_expr(expr_t *expr, size_t line, size_t column);
/* Create a function call expression. */
expr_t *ast_make_call(const char *name, expr_t **args, size_t arg_count,
                      size_t line, size_t column);
/* Create a compound literal expression. */
expr_t *ast_make_compound(type_kind_t type, size_t array_size,
                          size_t elem_size, expr_t *init,
                          init_entry_t *init_list, size_t init_count,
                          size_t line, size_t column);

/* Recursively free an expression tree. */
void ast_free_expr(expr_t *expr);

#endif /* VC_AST_EXPR_H */

/*
 * Expression tree cloning helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_AST_CLONE_H
#define VC_AST_CLONE_H

#include "ast.h"

/* Recursively clone an expression tree. Returns NULL on allocation failure. */
expr_t *clone_expr(const expr_t *expr);

#endif /* VC_AST_CLONE_H */

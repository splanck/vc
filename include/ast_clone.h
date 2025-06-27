/*
 * Expression tree cloning helpers.
 *
 * This interface provides ``clone_expr'' which performs a deep copy of an
 * expression node tree as defined in ``ast.h''.  Each node of the AST is
 * allocated dynamically and linked together via pointers, so cloning must
 * recursively duplicate every referenced node.
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

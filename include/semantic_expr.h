/*
 * Expression semantic analysis helpers.
 * Provides functions to validate expressions of all forms and
 * emit IR instructions for their evaluation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_EXPR_H
#define VC_SEMANTIC_EXPR_H

#include "ast.h"
#include "ir_core.h"
#include "symtable.h"

type_kind_t check_expr(expr_t *expr, symtable_t *vars, symtable_t *funcs,
                       ir_builder_t *ir, ir_value_t *out);

#endif /* VC_SEMANTIC_EXPR_H */

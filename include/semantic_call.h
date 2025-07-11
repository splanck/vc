/*
 * Function call expression semantic helper.
 * Provides the routine that checks call expressions and
 * generates the IR call instruction.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_CALL_H
#define VC_SEMANTIC_CALL_H

#include "ast_expr.h"
#include "ir_core.h"
#include "symtable.h"

type_kind_t check_call_expr(expr_t *expr, symtable_t *vars,
                            symtable_t *funcs, ir_builder_t *ir,
                            ir_value_t *out);

#endif /* VC_SEMANTIC_CALL_H */

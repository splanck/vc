/*
 * Cast and type conversion helpers.
 * Implements semantic checks for explicit casts between primitive types.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_expr.h"
#include "consteval.h"
#include "error.h"

/*
 * Validate a type cast expression. The operand expression is evaluated
 * and checked for compatibility with the destination type. No IR is
 * emitted for the conversion itself as primitive types share the same
 * representation.
 */
type_kind_t check_cast_expr(expr_t *expr, symtable_t *vars,
                            symtable_t *funcs, ir_builder_t *ir,
                            ir_value_t *out)
{
    ir_value_t val;
    type_kind_t src = check_expr(expr->cast.expr, vars, funcs, ir, &val);
    type_kind_t dst = expr->cast.type;

    if (src == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;

    if (src == dst ||
        ((is_intlike(src) || src == TYPE_PTR) &&
         (is_intlike(dst) || dst == TYPE_PTR)) ||
        (is_floatlike(src) && (is_floatlike(dst) || is_intlike(dst))) ||
        (is_floatlike(dst) && is_intlike(src)) ||
        (is_complexlike(src) && src == dst)) {
        if (out)
            *out = val;
        return dst;
    }

    error_set(expr->line, expr->column, error_current_file,
              error_current_function);
    if (out)
        *out = (ir_value_t){0};
    return TYPE_UNKNOWN;
}


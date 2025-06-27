/*
 * Function call expression semantic helper.
 * Validates calls against function prototypes and emits the
 * IR instruction performing the call.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic_call.h"
#include "semantic_expr.h"
#include "consteval.h"
#include "symtable.h"
#include "util.h"
#include "label.h"
#include "error.h"
#include <limits.h>

/*
 * Validate a function call by checking argument types against the
 * function's prototype and emit the IR instruction that performs
 * the call and captures its result.
 */
type_kind_t check_call_expr(expr_t *expr, symtable_t *vars,
                            symtable_t *funcs, ir_builder_t *ir,
                            ir_value_t *out)
{
    (void)vars;
    symbol_t *fsym = symtable_lookup(funcs, expr->call.name);
    if (!fsym) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    if (fsym->param_count != expr->call.arg_count) {
        error_set(expr->line, expr->column);
        return TYPE_UNKNOWN;
    }
    ir_value_t *vals = NULL;
    if (expr->call.arg_count) {
        vals = malloc(expr->call.arg_count * sizeof(*vals));
        if (!vals)
            return TYPE_UNKNOWN;
    }
    for (size_t i = 0; i < expr->call.arg_count; i++) {
        type_kind_t at = check_expr(expr->call.args[i], vars, funcs, ir,
                                    &vals[i]);
        type_kind_t pt = fsym->param_types[i];
        if (!(((is_intlike(pt) && is_intlike(at)) ||
               (is_floatlike(pt) && is_floatlike(at))) || at == pt)) {
            error_set(expr->call.args[i]->line, expr->call.args[i]->column);
            free(vals);
            return TYPE_UNKNOWN;
        }
    }
    for (size_t i = expr->call.arg_count; i > 0; i--)
        ir_build_arg(ir, vals[i - 1]);
    free(vals);
    ir_value_t call_val = ir_build_call(ir, expr->call.name,
                                       expr->call.arg_count);
    if (out)
        *out = call_val;
    return fsym->type;
}


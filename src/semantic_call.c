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
    symbol_t *fsym = symtable_lookup(funcs, expr->call.name);
    int via_ptr = 0;
    ir_value_t func_val;
    if (!fsym) {
        fsym = symtable_lookup(vars, expr->call.name);
        if (!fsym || fsym->type != TYPE_PTR ||
            fsym->func_ret_type == TYPE_UNKNOWN) {
            error_set(expr->line, expr->column);
            return TYPE_UNKNOWN;
        }
        via_ptr = 1;
        func_val = ir_build_load(ir, fsym->ir_name);
    }
    size_t expected = via_ptr ? fsym->func_param_count : fsym->param_count;
    int variadic = via_ptr ? fsym->func_variadic : fsym->is_variadic;
    type_kind_t *ptypes = via_ptr ? fsym->func_param_types : fsym->param_types;

    if ((!variadic && expected != expr->call.arg_count) ||
        (variadic && expr->call.arg_count < expected)) {
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
        if (i < expected) {
            type_kind_t pt = ptypes[i];
            if (!(((is_intlike(pt) && is_intlike(at)) ||
                   (is_floatlike(pt) && is_floatlike(at))) || at == pt)) {
                error_set(expr->call.args[i]->line, expr->call.args[i]->column);
                free(vals);
                return TYPE_UNKNOWN;
            }
        }
    }
    for (size_t i = expr->call.arg_count; i > 0; i--)
        ir_build_arg(ir, vals[i - 1]);
    free(vals);
    ir_value_t call_val = via_ptr
        ? ir_build_call_ptr(ir, func_val, expr->call.arg_count)
        : ir_build_call(ir, expr->call.name, expr->call.arg_count);
    if (out)
        *out = call_val;
    return via_ptr ? fsym->func_ret_type : fsym->type;
}


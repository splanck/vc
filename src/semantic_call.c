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
#include "semantic.h"
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
    symbol_t *fsym = symtable_lookup(funcs, expr->data.call.name);
    int via_ptr = 0;
    ir_value_t func_val;
    if (!fsym) {
        fsym = symtable_lookup(vars, expr->data.call.name);
        if (!fsym || fsym->type != TYPE_PTR ||
            fsym->func_ret_type == TYPE_UNKNOWN) {
            error_set(&error_ctx,expr->line, expr->column, NULL, NULL);
            return TYPE_UNKNOWN;
        }
        via_ptr = 1;
        func_val = ir_build_load(ir, fsym->ir_name);
    }
    size_t expected = via_ptr ? fsym->func_param_count : fsym->param_count;
    int variadic = via_ptr ? fsym->func_variadic : fsym->is_variadic;
    type_kind_t *ptypes = via_ptr ? fsym->func_param_types : fsym->param_types;

    if ((!variadic && expected != expr->data.call.arg_count) ||
        (variadic && expr->data.call.arg_count < expected)) {
        error_set(&error_ctx,expr->line, expr->column, NULL, NULL);
        return TYPE_UNKNOWN;
    }
    ir_value_t *vals = NULL;
    type_kind_t *atypes = NULL;
    if (expr->data.call.arg_count) {
        vals = malloc(expr->data.call.arg_count * sizeof(*vals));
        atypes = malloc(expr->data.call.arg_count * sizeof(*atypes));
        if (!vals || !atypes) {
            free(vals);
            free(atypes);
            return TYPE_UNKNOWN;
        }
    }
    for (size_t i = 0; i < expr->data.call.arg_count; i++) {
        type_kind_t at = check_expr(expr->data.call.args[i], vars, funcs, ir,
                                    &vals[i]);
        atypes[i] = at;
        if (at == TYPE_UNKNOWN) {
            free(vals);
            free(atypes);
            return TYPE_UNKNOWN;
        }
        if (i < expected) {
            type_kind_t pt = ptypes[i];
            if (!(((is_intlike(pt) && is_intlike(at)) ||
                   (is_floatlike(pt) && is_floatlike(at))) || at == pt)) {
                error_set(&error_ctx,expr->data.call.args[i]->line, expr->data.call.args[i]->column, NULL, NULL);
                free(vals);
                free(atypes);
                return TYPE_UNKNOWN;
            }
        }
    }
    if (semantic_get_x86_64()) {
        for (size_t i = 0; i < expr->data.call.arg_count; i++) {
            type_kind_t at = atypes[i];
            if (i >= expected &&
                (at == TYPE_FLOAT || at == TYPE_DOUBLE || at == TYPE_LDOUBLE)) {
                ir_build_arg(ir, vals[i], at);
            } else {
                ir_build_arg(ir, vals[i], at);
            }
        }
    } else {
        for (size_t i = expr->data.call.arg_count; i > 0; i--) {
            size_t idx = i - 1;
            type_kind_t at = atypes[idx];
            if (idx >= expected &&
                (at == TYPE_FLOAT || at == TYPE_DOUBLE || at == TYPE_LDOUBLE)) {
                ir_build_arg(ir, vals[idx], at);
            } else {
                ir_build_arg(ir, vals[idx], at);
            }
        }
    }
    free(vals);
    free(atypes);
    type_kind_t ret_type = via_ptr ? fsym->func_ret_type : fsym->type;
    int is_aggr = ret_type == TYPE_STRUCT || ret_type == TYPE_UNION;
    ir_value_t ret_ptr;
    if (is_aggr) {
        ir_value_t sz = ir_build_const(ir, (int)fsym->ret_struct_size);
        ret_ptr = ir_build_alloca(ir, sz);
        ir_build_arg(ir, ret_ptr, TYPE_PTR);
    }
    ir_value_t call_val = via_ptr
        ? (fsym->is_noreturn
            ? ir_build_call_ptr_nr(ir, func_val, expr->data.call.arg_count + (is_aggr ? 1 : 0))
            : ir_build_call_ptr(ir, func_val, expr->data.call.arg_count + (is_aggr ? 1 : 0)))
        : (fsym->is_noreturn
            ? ir_build_call_nr(ir, expr->data.call.name, expr->data.call.arg_count + (is_aggr ? 1 : 0))
            : ir_build_call(ir, expr->data.call.name, expr->data.call.arg_count + (is_aggr ? 1 : 0)));
    if (out)
        *out = is_aggr ? ret_ptr : call_val;
    (void)call_val;
    return ret_type;
}


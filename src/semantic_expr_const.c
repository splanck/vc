/*
 * Literal and constant expression helpers.
 * Validates number, string, character and complex literals and
 * emits the corresponding constant IR values.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include "semantic_expr.h"
#include "error.h"
#include "ir_core.h"

/*
 * Validate a numeric literal and emit a constant IR value.  The returned
 * type depends on the literal's size.
 */
type_kind_t check_number_expr(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out)
{
    (void)vars; (void)funcs;
    errno = 0;
    long long val = strtoll(expr->data.number.value, NULL, 0);
    if (errno != 0) {
        error_set(expr->line, expr->column, error_current_file,
                  error_current_function);
        if (out)
            *out = (ir_value_t){0};
        return TYPE_UNKNOWN;
    }
    if (out)
        *out = ir_build_const(ir, val);
    if (expr->data.number.long_count == 2)
        return expr->data.number.is_unsigned ? TYPE_ULLONG : TYPE_LLONG;
    if (expr->data.number.long_count == 1)
        return expr->data.number.is_unsigned ? TYPE_ULONG : TYPE_LONG;
    if (expr->data.number.is_unsigned)
        return TYPE_UINT;
    if (val > INT_MAX || val < INT_MIN)
        return TYPE_LLONG;
    return TYPE_INT;
}

/*
 * Validate a string literal and build its constant representation in the IR.
 * The resulting value has pointer type.
 */
type_kind_t check_string_expr(expr_t *expr, symtable_t *vars,
                              symtable_t *funcs, ir_builder_t *ir,
                              ir_value_t *out)
{
    (void)vars; (void)funcs;
    const char *text = expr->data.string.value;
    if (out) {
        if (expr->data.string.is_wide)
            *out = ir_build_wstring(ir, text);
        else
            *out = ir_build_string(ir, text);
    }
    return TYPE_PTR;
}

/*
 * Validate a character literal and emit a constant integer IR value.
 */
type_kind_t check_char_expr(expr_t *expr, symtable_t *vars,
                           symtable_t *funcs, ir_builder_t *ir,
                           ir_value_t *out)
{
    (void)vars; (void)funcs;
    if (out)
        *out = ir_build_const(ir, (int)expr->data.ch.value);
    return expr->data.ch.is_wide ? TYPE_INT : TYPE_CHAR;
}

/*
 * Validate a complex literal and emit a constant IR value.
 */
type_kind_t check_complex_literal(expr_t *expr, symtable_t *vars,
                                  symtable_t *funcs, ir_builder_t *ir,
                                  ir_value_t *out)
{
    (void)vars; (void)funcs;
    if (out)
        *out = ir_build_cplx_const(ir, expr->data.complex_lit.real,
                                   expr->data.complex_lit.imag);
    return TYPE_DOUBLE_COMPLEX;
}


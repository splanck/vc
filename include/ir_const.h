/*
 * Constant and literal IR builders.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_CONST_H
#define VC_IR_CONST_H

#include "ir_core.h"

/* Emit IR_CONST for `value` and return the resulting value id. */
ir_value_t ir_build_const(ir_builder_t *b, long long value);

/* Emit IR_CPLX_CONST building a complex literal. */
ir_value_t ir_build_cplx_const(ir_builder_t *b, double real, double imag);

/* Define a global string literal and return its value id (IR_GLOB_STRING).
 * `len` specifies the number of characters excluding the trailing NUL byte.
 */
ir_value_t ir_build_string(ir_builder_t *b, const char *data, size_t len);

/* Define a global wide string literal and return its value id (IR_GLOB_WSTRING). */
ir_value_t ir_build_wstring(ir_builder_t *b, const char *data);

#endif /* VC_IR_CONST_H */

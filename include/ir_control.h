/*
 * Control-flow and call IR builder helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_CONTROL_H
#define VC_IR_CONTROL_H

#include "ir_core.h"

/* Push `val` as an argument via IR_ARG. The argument's type is stored in imm. */
void ir_build_arg(ir_builder_t *b, ir_value_t val, type_kind_t type);

/* Emit IR_RETURN of `val` with result type `type`. */
void ir_build_return(ir_builder_t *b, ir_value_t val, type_kind_t type);

/* Emit IR_RETURN_AGG writing the aggregate pointed to by `ptr` of type `type`. */
void ir_build_return_agg(ir_builder_t *b, ir_value_t ptr, type_kind_t type);

/* Emit IR_CALL to `name` expecting `arg_count` previously pushed args. */
ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count);

ir_value_t ir_build_call_nr(ir_builder_t *b, const char *name, size_t arg_count);

ir_value_t ir_build_call_ptr(ir_builder_t *b, ir_value_t func, size_t arg_count);

ir_value_t ir_build_call_ptr_nr(ir_builder_t *b, ir_value_t func,
                                size_t arg_count);

/* Mark the start of a function with IR_FUNC_BEGIN. */
ir_instr_t *ir_build_func_begin(ir_builder_t *b, const char *name);

/* Mark the end of the current function with IR_FUNC_END. */
void ir_build_func_end(ir_builder_t *b);

/* Emit IR_BR jumping to `label`. */
void ir_build_br(ir_builder_t *b, const char *label);

/* Emit IR_BCOND using `cond` to branch to `label`. */
void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label);

/* Emit IR_LABEL marking the current position as `label`. */
void ir_build_label(ir_builder_t *b, const char *label);

#endif /* VC_IR_CONTROL_H */

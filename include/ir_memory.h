/*
 * Memory-related IR builder helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_MEMORY_H
#define VC_IR_MEMORY_H

#include "ir_core.h"

/* Emit IR_LOAD of variable `name`. */
ir_value_t ir_build_load(ir_builder_t *b, const char *name, type_kind_t type);

/* Emit a volatile IR_LOAD of variable `name`. */
ir_value_t ir_build_load_vol(ir_builder_t *b, const char *name, type_kind_t type);

/* Emit IR_STORE of `val` into variable `name`. */
void ir_build_store(ir_builder_t *b, const char *name, type_kind_t type,
                    ir_value_t val);

/* Emit a volatile IR_STORE of `val` into variable `name`. */
void ir_build_store_vol(ir_builder_t *b, const char *name, type_kind_t type,
                        ir_value_t val);

/* Load function parameter `index` via IR_LOAD_PARAM. */
ir_value_t ir_build_load_param(ir_builder_t *b, int index, type_kind_t type);

/* Store `val` into parameter slot `index` using IR_STORE_PARAM. */
void ir_build_store_param(ir_builder_t *b, int index, type_kind_t type,
                          ir_value_t val);

/* Obtain the address of variable `name` via IR_ADDR. */
ir_value_t ir_build_addr(ir_builder_t *b, const char *name);

/* Emit IR_LOAD_PTR from pointer `addr`. */
ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr);

/* Restrict-qualified pointer load. */
ir_value_t ir_build_load_ptr_res(ir_builder_t *b, ir_value_t addr);

/* Emit IR_STORE_PTR writing `val` to pointer `addr`. */
void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val);

/* Restrict-qualified pointer store. */
void ir_build_store_ptr_res(ir_builder_t *b, ir_value_t addr, ir_value_t val);

/* Emit IR_PTR_ADD adding `idx` (scaled by `elem_size`) to `ptr`. */
ir_value_t ir_build_ptr_add(ir_builder_t *b, ir_value_t ptr, ir_value_t idx,
                            int elem_size);

/* Emit IR_PTR_DIFF computing `a - bptr` in elements of size `elem_size`.
 * A zero `elem_size` yields a result of zero. */
ir_value_t ir_build_ptr_diff(ir_builder_t *b, ir_value_t a, ir_value_t bptr,
                             int elem_size);

/* Load element `name[idx]` using IR_LOAD_IDX. */
ir_value_t ir_build_load_idx(ir_builder_t *b, const char *name, ir_value_t idx,
                             type_kind_t type);

/* Volatile version of IR_LOAD_IDX. */
ir_value_t ir_build_load_idx_vol(ir_builder_t *b, const char *name,
                                 ir_value_t idx, type_kind_t type);

/* Store `val` into `name[idx]` using IR_STORE_IDX. */
void ir_build_store_idx(ir_builder_t *b, const char *name, ir_value_t idx,
                        ir_value_t val, type_kind_t type);

/* Volatile version of IR_STORE_IDX. */
void ir_build_store_idx_vol(ir_builder_t *b, const char *name, ir_value_t idx,
                            ir_value_t val, type_kind_t type);

/* Emit IR_ALLOCA reserving `size` bytes on the stack. */
ir_value_t ir_build_alloca(ir_builder_t *b, ir_value_t size);

#endif /* VC_IR_MEMORY_H */

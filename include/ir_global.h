/*
 * Builders for global variables and aggregates.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_GLOBAL_H
#define VC_IR_GLOBAL_H

#include "ir_core.h"

/* Emit IR_GLOB_VAR to define `name` with optional initial `value`. */
void ir_build_glob_var(ir_builder_t *b, const char *name, long long value,
                       int is_static);

/*
 * Emit IR_GLOB_ARRAY defining `name` with `count` initial values. The
 * values are copied into the instruction and `is_static` controls
 * linkage.
 */
void ir_build_glob_array(ir_builder_t *b, const char *name,
                         const long long *values, size_t count,
                         int is_static);

/* Begin a global union using IR_GLOB_UNION. `size` specifies the type size. */
void ir_build_glob_union(ir_builder_t *b, const char *name, int size,
                         int is_static);

/* Begin a global struct using IR_GLOB_STRUCT with the given size. */
void ir_build_glob_struct(ir_builder_t *b, const char *name, int size,
                          int is_static);

#endif /* VC_IR_GLOBAL_H */

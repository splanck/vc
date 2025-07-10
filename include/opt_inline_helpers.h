/*
 * Helper functions for the inline optimization pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_OPT_INLINE_HELPERS_H
#define VC_OPT_INLINE_HELPERS_H

#include "ir_core.h"

/* Representation of a candidate inline function */
typedef struct {
    const char *name;    /* function name */
    ir_instr_t *body;    /* cloned body instructions */
    size_t count;        /* number of instructions in body */
    int param_count;     /* number of parameters */
} inline_func_t;

/*
 * Clone a small function body for inlining.
 * `begin` must point to an IR_FUNC_BEGIN instruction.
 * On success, returns 1 and stores a newly allocated instruction array in
 * `*out` and the number of entries in `*count`.
 */
int clone_inline_body(ir_instr_t *begin, ir_instr_t **out, size_t *count);

/*
 * Collect inline function candidates from the builder.
 * On success, returns 1 and stores an array of inline_func_t in `*out` with
 * `*count` entries.
 */
int collect_funcs(ir_builder_t *ir, inline_func_t **out, size_t *count);

#endif /* VC_OPT_INLINE_HELPERS_H */

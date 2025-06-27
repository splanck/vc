/*
 * Register allocation interface.
 *
 * The allocator maps IR value identifiers to either a physical
 * register or a stack slot. A linear scan over the instruction
 * stream decides when to assign registers and when to spill
 * values to the stack.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_REGALLOC_H
#define VC_REGALLOC_H

#include "ir_core.h"

/* Location mapping for IR values */
typedef struct {
    int *loc;       /* >=0 register index, <0 stack slot (-n) */
    int stack_slots;/* number of stack slots used */
} regalloc_t;

/*
 * Assign locations to IR values using a linear scan algorithm.
 *
 * Registers are allocated from a fixed pool. When none are
 * available the value is placed in a new stack slot. A register
 * becomes free again once the allocator reaches the last
 * instruction that references the value stored in it.
 */
void regalloc_run(ir_builder_t *ir, regalloc_t *ra);

/*
 * Free any memory associated with the allocator.
 *
 * This does not modify the IR but simply releases the location
 * table produced by `regalloc_run`.
 */
void regalloc_free(regalloc_t *ra);


#endif /* VC_REGALLOC_H */

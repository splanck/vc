/*
 * Register allocation interface.
 *
 * The allocator assigns each SSA value produced by the IR to either a
 * physical register or a stack slot.  Allocation is performed using a
 * simple linear scan: values are given registers from a small fixed pool
 * and spilled to the stack when no registers remain.  Registers are
 * recycled immediately after the last use of a value, allowing them to
 * be reused later in the instruction stream.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_REGALLOC_H
#define VC_REGALLOC_H

#include "ir_core.h"

/*
 * Indices of scratch registers kept out of the general allocation pool.
 *
 * REGALLOC_SCRATCH_REG is always available for temporary values. Some
 * store operations may require a second temporary register which is
 * provided by REGALLOC_SCRATCH_REG2.
 */
#define REGALLOC_SCRATCH_REG  0
#define REGALLOC_SCRATCH_REG2 1

/*
 * Location mapping for IR values returned by the allocator.
 *
 * `loc[i]` holds the location assigned to value `i`.  Non-negative
 * numbers correspond to a physical register index while negative
 * numbers encode a stack slot number (\-n).  `stack_slots` reports how
 * many stack slots were required in total.
 */
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
/*
 * Run the allocator on the given builder and populate `ra` with the
 * computed location map.  `ir` must contain the finalized list of IR
 * instructions.
 */
void regalloc_run(ir_builder_t *ir, regalloc_t *ra);

/*
 * Free any memory associated with the allocator.
 *
 * This does not modify the IR but simply releases the location
 * table produced by `regalloc_run`.
 */
/* Release all memory held inside `ra`. */
void regalloc_free(regalloc_t *ra);


#endif /* VC_REGALLOC_H */

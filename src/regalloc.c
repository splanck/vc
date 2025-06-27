/*
 * Linear scan register allocator.
 *
 * This allocator walks the IR once assigning every value either a
 * register or a stack slot.  A small array of physical registers forms
 * the free pool; when it is exhausted new values are "spilled" to the
 * stack.  Lifetimes are tracked by computing the last instruction that
 * references each value so that registers can be released as soon as a
 * value is no longer needed.
 *
 * Each allocated location is stored in the `regalloc_t` structure where
 * non-negative entries represent a physical register and negative values
 * encode a stack slot number (\-n).  The table is later used by the code
 * generator to decide whether to emit register or memory operands.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "regalloc.h"
#include "regalloc_x86.h"

#define SCRATCH_REG (REGALLOC_NUM_REGS - 1)
#define NUM_REGS REGALLOC_NUM_REGS
#define NUM_ALLOC_REGS (REGALLOC_NUM_REGS - 1)

/*
 * Compute the "last use" position for every value in the IR.
 *
 * The instruction stream is scanned exactly once.  Whenever a value
 * appears as a source operand the index of the current instruction is
 * recorded.  At the end of the scan each entry holds the index of the
 * final instruction that references the value (or -1 if the value is
 * never used).  The resulting array is indexed by value id and should
 * be freed by the caller.  NULL is returned on allocation failure.
 */
static int *compute_last_use(ir_builder_t *ir, int max_id)
{
    int *last = malloc((size_t)max_id * sizeof(int));
    if (!last)
        return NULL;
    for (int i = 0; i < max_id; i++)
        last[i] = -1;

    int idx = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next, idx++) {
        if (ins->src1 > 0 && ins->src1 < max_id)
            last[ins->src1] = idx;
        if (ins->src2 > 0 && ins->src2 < max_id)
            last[ins->src2] = idx;
    }
    return last;
}

/*
 * Populate `ra` with locations for every value defined in `ir`.
 *
 * The allocator performs a linear pass over the IR instructions.
 * When a new value is defined a register is taken from the free
 * pool if available; otherwise a new stack slot is allocated.  As
 * soon as the current instruction index matches the pre-computed
 * last-use index of a value its register is returned to the pool
 * for reuse by later instructions.  This strategy ensures that
 * register pressure remains low while avoiding complex live range
 * analysis.
 */
void regalloc_run(ir_builder_t *ir, regalloc_t *ra)
{
    int max_id = ir->next_value_id;
    ra->loc = malloc((size_t)max_id * sizeof(int));
    ra->stack_slots = 0;
    if (!ra->loc)
        return;
    for (int i = 0; i < max_id; i++)
        ra->loc[i] = -1;

    int *last = compute_last_use(ir, max_id);
    if (!last)
        return;

    int free_regs[NUM_ALLOC_REGS];
    int free_count = NUM_ALLOC_REGS;
    for (int i = 0; i < NUM_ALLOC_REGS; i++)
        free_regs[i] = NUM_ALLOC_REGS - 1 - i; /* allocate from high to low */

    int idx = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next, idx++) {
        /* Allocate a location for the destination value */
        if (ins->dest > 0 && ins->dest < max_id && ra->loc[ins->dest] == -1) {
            if (free_count > 0) {
                /* grab a free register */
                ra->loc[ins->dest] = free_regs[--free_count];
            } else {
                /* spill to a new stack slot */
                ra->loc[ins->dest] = -(++ra->stack_slots);
            }
        }

        /* Return registers once their values are no longer needed */
        if (ins->src1 > 0 && ins->src1 < max_id &&
            ra->loc[ins->src1] >= 0 && last[ins->src1] == idx) {
            free_regs[free_count++] = ra->loc[ins->src1];
        }
        if (ins->src2 > 0 && ins->src2 < max_id &&
            ra->loc[ins->src2] >= 0 && last[ins->src2] == idx) {
            free_regs[free_count++] = ra->loc[ins->src2];
        }
        if (ins->dest > 0 && ins->dest < max_id &&
            ra->loc[ins->dest] >= 0 && last[ins->dest] == idx) {
            free_regs[free_count++] = ra->loc[ins->dest];
        }
    }
    free(last);
}

/*
 * Release any memory allocated during `regalloc_run`.
 *
 * This simply frees the location table and resets the bookkeeping
 * fields.  It does not alter the IR itself.
 */
void regalloc_free(regalloc_t *ra)
{
    free(ra->loc);
    ra->loc = NULL;
    ra->stack_slots = 0;
}

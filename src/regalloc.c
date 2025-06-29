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
static int *compute_last_use(ir_builder_t *ir, size_t max_id)
{
    int *last = malloc(max_id * sizeof(int));
    if (!last)
        return NULL;
    for (size_t i = 0; i < max_id; i++)
        last[i] = -1;

    int idx = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next, idx++) {
        if (ins->src1 > 0 && (size_t)ins->src1 < max_id)
            last[ins->src1] = idx;
        if (ins->src2 > 0 && (size_t)ins->src2 < max_id)
            last[ins->src2] = idx;
    }
    return last;
}

/*
 * Assign a register or stack slot to the destination of `ins`.
 *
 * The allocator pulls registers from the `free_regs` stack until it runs
 * out, at which point new stack slots are allocated sequentially.  The
 * destination value of `ins` only receives a location if it has a valid id
 * and none was previously assigned.
 */
static void allocate_location(ir_instr_t *ins, int *free_regs, int *free_count,
                              regalloc_t *ra)
{
    int id = ins->dest;
    if (id <= 0 || ra->loc[id] != -1)
        return;

    if (*free_count > 0) {
        ra->loc[id] = free_regs[--(*free_count)];
    } else {
        ra->loc[id] = -(++ra->stack_slots);
    }
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
    /*
     * Allocation proceeds in a single linear pass.  First the last use of
     * every value is computed so registers can be released as soon as the
     * allocator walks past that point.  During the scan each destination is
     * given a register from the free stack or spilled to a new stack slot if
     * none remain.  Freed registers are pushed back onto the stack for reuse.
     */

    size_t max_id = ir->next_value_id;
    ra->loc = malloc(max_id * sizeof(int));
    ra->stack_slots = 0;
    if (!ra->loc)
        return;
    for (size_t i = 0; i < max_id; i++)
        ra->loc[i] = -1;

    int *last = compute_last_use(ir, max_id);
    if (!last) {
        free(ra->loc);
        ra->loc = NULL;
        return;
    }

    int free_regs[NUM_ALLOC_REGS];
    int free_count = NUM_ALLOC_REGS;
    for (int i = 0; i < NUM_ALLOC_REGS; i++)
        free_regs[i] = NUM_ALLOC_REGS - 1 - i; /* allocate from high to low */

    int idx = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next, idx++) {
        /* assign the destination a register or stack slot */
        allocate_location(ins, free_regs, &free_count, ra);

        /* release registers whose value will not be needed again */
        if (ins->src1 > 0 && (size_t)ins->src1 < max_id &&
            ra->loc[ins->src1] >= 0 && last[ins->src1] == idx)
            free_regs[free_count++] = ra->loc[ins->src1];

        if (ins->src2 > 0 && (size_t)ins->src2 < max_id &&
            ra->loc[ins->src2] >= 0 && last[ins->src2] == idx)
            free_regs[free_count++] = ra->loc[ins->src2];

        if (ins->dest > 0 && (size_t)ins->dest < max_id &&
            ra->loc[ins->dest] >= 0 && last[ins->dest] == idx)
            free_regs[free_count++] = ra->loc[ins->dest];
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

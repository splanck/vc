/*
 * Linear scan register allocator.
 *
 * Each IR value is mapped to one of a small set of registers or to a
 * stack slot when all registers are in use. The allocator performs a
 * single pass over the instruction stream and releases registers when
 * the last use of a value is reached.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "regalloc.h"
#include "regalloc_x86.h"

#define NUM_REGS REGALLOC_NUM_REGS

/*
 * Record the index of the final instruction that references each value.
 * Returns an array indexed by value id or NULL on allocation failure.
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
 * The allocator walks the instruction list once. New values are
 * assigned a free register if one exists; otherwise a fresh stack
 * slot number is used. The `last` array records when each value is
 * seen for the final time so its register can be returned to the
 * free pool.
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

    int free_regs[NUM_REGS];
    int free_count = NUM_REGS;
    for (int i = 0; i < NUM_REGS; i++)
        free_regs[i] = NUM_REGS - 1 - i; /* allocate from high to low */

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

/* Release any memory allocated during `regalloc_run`. */
void regalloc_free(regalloc_t *ra)
{
    free(ra->loc);
    ra->loc = NULL;
    ra->stack_slots = 0;
}

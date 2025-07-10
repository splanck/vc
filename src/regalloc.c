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
static void assign_destination_location(ir_instr_t *ins, int *free_regs,
                                        int *free_count, regalloc_t *ra,
                                        int ret_reg_active)
{
    int id = ins->dest;
    if (id <= 0 || ra->loc[id] != -1)
        return;

    if (ret_reg_active && ins->op == IR_LOAD_PARAM && ins->imm == 0) {
        ra->loc[id] = REGALLOC_RET_REG;
        return;
    }

    if (*free_count > 0) {
        ra->loc[id] = free_regs[--(*free_count)];
    } else {
        ra->loc[id] = -(++ra->stack_slots);
    }
}

/*
 * Initialize the stack of available registers.
 */
static void setup_free_registers(int *free_regs, int *free_count,
                                 int ret_reg_active)
{
    *free_count = 0;
    for (int i = 0; i < NUM_ALLOC_REGS; i++) {
        int r = NUM_ALLOC_REGS - 1 - i; /* allocate from high to low */
        if (ret_reg_active && r == REGALLOC_RET_REG)
            continue;
        free_regs[(*free_count)++] = r;
    }
}

/*
 * Release registers whose value will not be needed again.
 */
static void release_unused_regs(ir_instr_t *ins, int idx, int *last,
                                size_t max_id, regalloc_t *ra,
                                int *free_regs, int *free_count)
{
    if (ins->src1 > 0 && (size_t)ins->src1 < max_id &&
        ra->loc[ins->src1] >= 0 && last[ins->src1] == idx)
        free_regs[(*free_count)++] = ra->loc[ins->src1];

    if (ins->src2 > 0 && (size_t)ins->src2 < max_id &&
        ra->loc[ins->src2] >= 0 && last[ins->src2] == idx)
        free_regs[(*free_count)++] = ra->loc[ins->src2];

    if (ins->dest > 0 && (size_t)ins->dest < max_id &&
        ra->loc[ins->dest] >= 0 && last[ins->dest] == idx)
        free_regs[(*free_count)++] = ra->loc[ins->dest];
}

/*
 * Populate `ra` with locations for every value defined in `ir`.
 *
 * Lifetimes are determined by `compute_last_use` which walks the IR once and
 * records the index of the final instruction that touches each value.  During
 * allocation this table is consulted so that when the scan reaches a value's
 * last use its register can immediately be freed.
 *
 * The free register pool is maintained as a simple stack.  All allocatable
 * registers are pushed in descending order and popped whenever a new value is
 * defined.  When a value dies its register is pushed back for reuse.  The
 * optional return register is skipped when necessary so the scratch register is
 * always available.
 *
 * If the stack runs out, new values are spilled to sequential stack slots.
 * Each slot increments `ra->stack_slots` and is encoded as a negative number in
 * `ra->loc` (-(slot + 1)).  Stack slots persist for the duration of the scan
 * but their contents may be reloaded by later passes.
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

    int ret_reg_active = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        if (it->op == IR_RETURN_AGG) {
            ret_reg_active = 1;
            break;
        }

    int free_regs[NUM_ALLOC_REGS];
    int free_count = 0;
    setup_free_registers(free_regs, &free_count, ret_reg_active);

    int idx = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next, idx++) {
        /* assign the destination a register or stack slot */
        assign_destination_location(ins, free_regs, &free_count, ra,
                                    ret_reg_active);

        /* release registers whose value will not be needed again */
        release_unused_regs(ins, idx, last, max_id, ra, free_regs,
                           &free_count);
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

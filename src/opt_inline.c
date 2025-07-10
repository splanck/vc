/*
 * Simple function inlining pass.
 *
 * Replaces calls to small functions with short bodies.  Functions
 * consisting of two parameter loads and a single arithmetic operation
 * are currently inlined directly into the call site.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include "opt.h"
#include "error.h"
#include "util.h"
#include "opt_inline_helpers.h"


/* Remove instruction at list[index] from both array and linked list */
static void remove_instr(ir_builder_t *ir, ir_instr_t **list, int *count, int index)
{
    ir_instr_t *prev = (index > 0) ? list[index - 1] : NULL;
    ir_instr_t *ins = list[index];
    ir_instr_t *next = ins->next;
    if (prev)
        prev->next = next;
    else
        ir->head = next;
    if (ins == ir->tail)
        ir->tail = prev;
    free(ins->name);
    free(ins->data);
    free(ins);
    for (int i = index; i < *count - 1; i++)
        list[i] = list[i + 1];
    (*count)--;
}

/* Recompute tail pointer after all changes */
static void recompute_tail(ir_builder_t *ir)
{
    ir_instr_t *t = ir->head;
    if (!t) { ir->tail = NULL; return; }
    while (t->next) t = t->next;
    ir->tail = t;
}

typedef struct {
    int old_id;
    int new_id;
    ir_instr_t *ins;
} map_entry_t;

static int gather_call_args(ir_instr_t **list, int idx, int argc, int *args)
{
    for (int a = 0; a < argc; a++) {
        ir_instr_t *arg = list[idx - argc + a];
        if (!arg || arg->op != IR_ARG)
            return 0;
        args[a] = arg->src1;
    }
    return 1;
}

static void replace_value_uses(ir_instr_t **list, int start, int count,
                              int old, int new)
{
    for (int u = start; u < count; u++) {
        if (list[u]->src1 == old)
            list[u]->src1 = new;
        if (list[u]->src2 == old)
            list[u]->src2 = new;
    }
}

static int map_lookup(map_entry_t *map, size_t count, int old)
{
    for (size_t i = 0; i < count; i++)
        if (map[i].old_id == old)
            return map[i].new_id;
    return old;
}

static int insert_inline_body(ir_builder_t *ir, ir_instr_t *call,
                              inline_func_t *fn, int argc, int *args,
                              int *ret_val)
{
    map_entry_t map[32];
    size_t mcount = 0;
    ir_instr_t *pos = call;
    *ret_val = call->dest;

    for (size_t k = 0; k < fn->count; k++) {
        ir_instr_t *orig = &fn->body[k];
        if (orig->op == IR_LOAD_PARAM) {
            if (orig->imm >= argc)
                return 0;
            map[mcount++] = (map_entry_t){orig->dest, args[orig->imm], NULL};
            continue;
        }
        if (orig->op == IR_RETURN || orig->op == IR_RETURN_AGG) {
            *ret_val = map_lookup(map, mcount, orig->src1);
            for (size_t m = 0; m < mcount; m++) {
                if (map[m].old_id == orig->src1 && map[m].ins) {
                    map[m].ins->dest = call->dest;
                    map[m].new_id = call->dest;
                    *ret_val = call->dest;
                    break;
                }
            }
            continue;
        }

        ir_instr_t *ni = ir_insert_after(ir, pos);
        if (!ni)
            return 0;
        ni->op = orig->op;
        ni->imm = orig->imm;
        ni->src1 = map_lookup(map, mcount, orig->src1);
        ni->src2 = map_lookup(map, mcount, orig->src2);
        ni->name = NULL;
        ni->data = NULL;
        ni->is_volatile = 0;
        ni->dest = (int)ir->next_value_id++;
        map[mcount++] = (map_entry_t){orig->dest, ni->dest, ni};
        pos = ni;
    }

    return 1;
}

/* Build an array containing all IR instructions in order */
static ir_instr_t **gather_call_list(ir_builder_t *ir, int *count)
{
    *count = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        (*count)++;
    if (*count == 0)
        return NULL;

    ir_instr_t **list = malloc((size_t)(*count) * sizeof(*list));
    if (!list) {
        opt_error("out of memory");
        return NULL;
    }

    int idx = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        list[idx++] = it;

    return list;
}

/* Inline a single call instruction if it matches an eligible function */
static int inline_call(ir_builder_t *ir, ir_instr_t **list, int *count, int i,
                       inline_func_t *funcs, size_t func_count)
{
    ir_instr_t *ins = list[i];
    if (ins->op != IR_CALL)
        return 0;

    inline_func_t *fn = NULL;
    for (size_t j = 0; j < func_count; j++) {
        if (strcmp(funcs[j].name, ins->name) == 0) {
            fn = &funcs[j];
            break;
        }
    }
    if (!fn || ins->imm != fn->param_count || i < (int)fn->param_count)
        return 0;

    int argc = (int)fn->param_count;
    if (argc > 8)
        return 0; /* limit to small functions */

    int args[8];
    if (!gather_call_args(list, i, argc, args))
        return 0;

    for (int a = 0; a < argc; a++) {
        remove_instr(ir, list, count, i - 1);
        i--;
    }

    int ret_val;
    if (!insert_inline_body(ir, ins, fn, argc, args, &ret_val))
        return 0;

    replace_value_uses(list, i + 1, *count, ins->dest, ret_val);

    free(ins->name);
    ins->name = NULL;
    remove_instr(ir, list, count, i);
    return 1;
}

void inline_small_funcs(ir_builder_t *ir)
{
    if (!ir)
        return;

    inline_func_t *funcs = NULL;
    size_t func_count = 0;
    if (!collect_funcs(ir, &funcs, &func_count))
        return;

    int count = 0;
    ir_instr_t **list = gather_call_list(ir, &count);
    if (!list) {
        for (size_t j = 0; j < func_count; j++)
            free(funcs[j].body);
        free(funcs);
        return;
    }

    for (int i = 0; i < count; i++) {
        if (inline_call(ir, list, &count, i, funcs, func_count))
            i--; /* restart from previous position after modification */
    }

    recompute_tail(ir);
    free(list);
    for (size_t j = 0; j < func_count; j++)
        free(funcs[j].body);
    free(funcs);
}


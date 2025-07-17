/*
 * Alias analysis pass assigning alias sets to memory operations.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "ir_core.h"
#include "util.h"

static int lookup_alias(alias_ent_t **list, const char *name, int *next_id)
{
    alias_ent_t *e = *list;
    while (e && strcmp(e->name, name) != 0)
        e = e->next;
    if (e)
        return e->set;
    e = malloc(sizeof(*e));
    if (!e) {
        opt_error("out of memory");
        return 0;
    }
    e->name = vc_strdup(name);
    if (!e->name) {
        opt_error("out of memory");
        free(e);
        return 0;
    }
    e->set = (*next_id)++;
    e->next = *list;
    *list = e;
    return e->set;
}

/* Compute alias sets for memory instructions */
void compute_alias_sets(ir_builder_t *ir)
{
    if (!ir)
        return;

    alias_ent_t *vars = NULL;
    int next_id = 1;

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next)
        if (ins->alias_set >= next_id)
            next_id = ins->alias_set + 1;

    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        switch (ins->op) {
        case IR_LOAD:
        case IR_STORE:
        case IR_LOAD_IDX:
        case IR_STORE_IDX:
        case IR_BFLOAD:
        case IR_BFSTORE:
            if (ins->name && ins->alias_set == 0)
                ins->alias_set = lookup_alias(&vars, ins->name, &next_id);
            break;
        case IR_LOAD_PTR:
        case IR_STORE_PTR:
            if (ins->is_restrict && ins->alias_set == 0)
                ins->alias_set = next_id++;
            break;
        default:
            break;
        }
    }

    while (vars) {
        alias_ent_t *n = vars->next;
        free((char *)vars->name);
        free(vars);
        vars = n;
    }
}

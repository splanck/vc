/*
 * Unreachable block elimination pass.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "util.h"

/* Simple array for storing referenced labels */
typedef struct label_ref {
    char *name;
    struct label_ref *next;
} label_ref_t;

static int label_referenced(label_ref_t *list, const char *name)
{
    for (; list; list = list->next)
        if (strcmp(list->name, name) == 0)
            return 1;
    return 0;
}

static void free_label_refs(label_ref_t *list)
{
    while (list) {
        label_ref_t *n = list->next;
        free(list->name);
        free(list);
        list = n;
    }
}

/* Remove unreachable instructions within functions */
void remove_unreachable_blocks(ir_builder_t *ir)
{
    if (!ir)
        return;

    label_ref_t *labels = NULL;
    label_ref_t **label_tail = &labels;

    /* collect all branch target labels */
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_BR || ins->op == IR_BCOND) {
            label_ref_t *e = malloc(sizeof(*e));
            if (!e) {
                opt_error("out of memory");
                free_label_refs(labels);
                return;
            }
            e->name = vc_strdup(ins->name ? ins->name : "");
            if (!e->name) {
                opt_error("out of memory");
                free(e);
                free_label_refs(labels);
                return;
            }
            e->next = NULL;
            *label_tail = e;
            label_tail = &e->next;
        }
    }

    int in_func = 0;
    int reachable = 1;
    ir_instr_t *prev = NULL;
    ir_instr_t *cur = ir->head;

    while (cur) {
        ir_instr_t *next = cur->next;

        switch (cur->op) {
        case IR_FUNC_BEGIN:
            in_func = 1;
            reachable = 1;
            prev = cur;
            break;
        case IR_FUNC_END:
            in_func = 0;
            reachable = 1;
            prev = cur;
            break;
        case IR_BR:
            reachable = 0;
            prev = cur;
            break;
        case IR_RETURN:
        case IR_RETURN_AGG:
            reachable = 0;
            prev = cur;
            break;
        case IR_BCOND:
            /* record already done */
            prev = cur;
            break;
        case IR_LABEL:
            if (label_referenced(labels, cur->name))
                reachable = 1;
            if (!reachable) {
                if (prev)
                    prev->next = next;
                else
                    ir->head = next;
                if (ir->tail == cur)
                    ir->tail = prev;
                free(cur->name);
                free(cur->data);
                free(cur);
                cur = prev; /* compensate for increment */
            } else {
                prev = cur;
            }
            break;
        default:
            if (in_func && !reachable) {
                if (prev)
                    prev->next = next;
                else
                    ir->head = next;
                if (ir->tail == cur)
                    ir->tail = prev;
                free(cur->name);
                free(cur->data);
                free(cur);
                cur = prev; /* stay */
            } else {
                prev = cur;
            }
            break;
        }

        cur = next;
    }

    free_label_refs(labels);
}

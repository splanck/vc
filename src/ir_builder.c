#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "ir_builder.h"

/* Allocate and append a blank instruction */
ir_instr_t *append_instr(ir_builder_t *b)
{
    ir_instr_t *ins = calloc(1, sizeof(*ins));
    if (!ins)
        return NULL;
    ins->dest = -1;
    ins->name = NULL;
    ins->data = NULL;
    ins->is_volatile = 0;
    ins->is_restrict = 0;
    ins->alias_set = 0;
    ins->file = b->cur_file;
    ins->line = b->cur_line;
    ins->column = b->cur_column;
    if (!b->head)
        b->head = ins;
    else
        b->tail->next = ins;
    b->tail = ins;
    return ins;
}

/* Allocate the next value identifier */
int alloc_value_id(ir_builder_t *b)
{
    if (b->next_value_id >= (size_t)INT_MAX) {
        fprintf(stderr, "ir_core: too many values\n");
        exit(1);
    }
    return (int)b->next_value_id++;
}

/* Remove an instruction from the list */
void remove_instr(ir_builder_t *b, ir_instr_t *ins)
{
    if (!b || !ins)
        return;
    ir_instr_t *prev = NULL;
    ir_instr_t *cur = b->head;
    while (cur && cur != ins) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur)
        return;
    if (prev)
        prev->next = cur->next;
    else
        b->head = cur->next;
    if (b->tail == cur)
        b->tail = prev;
    free(cur->name);
    free(cur->data);
    free(cur);
}

/* Lookup or create an alias set for the given variable name */
int get_alias(ir_builder_t *b, const char *name)
{
    alias_ent_t *e = b->aliases;
    while (e && strcmp(e->name, name) != 0)
        e = e->next;
    if (e)
        return e->set;
    e = malloc(sizeof(*e));
    if (!e)
        return 0;
    e->name = name;
    e->set = b->next_alias_id++;
    e->next = b->aliases;
    b->aliases = e;
    return e->set;
}

/* Insert a blank instruction after `pos` */
ir_instr_t *ir_insert_after(ir_builder_t *b, ir_instr_t *pos)
{
    if (!b)
        return NULL;

    ir_instr_t *ins = calloc(1, sizeof(*ins));
    if (!ins)
        return NULL;
    ins->dest = -1;
    ins->name = NULL;
    ins->data = NULL;
    ins->is_volatile = 0;
    ins->is_restrict = 0;
    ins->alias_set = 0;
    ins->file = b->cur_file;
    ins->line = b->cur_line;
    ins->column = b->cur_column;

    if (!pos) {
        ins->next = b->head;
        b->head = ins;
        if (!b->tail)
            b->tail = ins;
    } else {
        ins->next = pos->next;
        pos->next = ins;
        if (b->tail == pos)
            b->tail = ins;
    }

    return ins;
}


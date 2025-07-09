#ifndef VC_IR_BUILDER_H
#define VC_IR_BUILDER_H

#include "ir_core.h"

typedef struct alias_ent {
    const char *name;
    int set;
    struct alias_ent *next;
} alias_ent_t;

/* Append a new blank instruction to the builder's list */
ir_instr_t *append_instr(ir_builder_t *b);

/* Allocate the next unique value id */
int alloc_value_id(ir_builder_t *b);

/* Remove instruction from the builder list */
void remove_instr(ir_builder_t *b, ir_instr_t *ins);

/* Get or create an alias set for a variable */
int get_alias(ir_builder_t *b, const char *name);

/* Insert a blank instruction after the given position */
ir_instr_t *ir_insert_after(ir_builder_t *b, ir_instr_t *pos);

#endif /* VC_IR_BUILDER_H */

#ifndef VC_REGALLOC_H
#define VC_REGALLOC_H

#include "ir.h"

/* Location mapping for IR values */
typedef struct {
    int *loc;       /* >=0 register index, <0 stack slot (-n) */
    int stack_slots;/* number of stack slots used */
} regalloc_t;

/* Assign locations to IR values using a simple linear scan allocator */
void regalloc_run(ir_builder_t *ir, regalloc_t *ra);

/* Free resources held by the allocator */
void regalloc_free(regalloc_t *ra);

/* Return physical register name for given index */
const char *regalloc_reg_name(int idx);

#endif /* VC_REGALLOC_H */

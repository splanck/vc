/*
 * Global variable and structure IR builders.
 *
 * These helpers emit IR directives that describe global data such as
 * variables, arrays and aggregates.  The emitted instructions are
 * appended to an ir_builder_t and later consumed by the code generator.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "ir_global.h"
#include "util.h"

/* Helper to append a blank instruction */
static ir_instr_t *append_instr(ir_builder_t *b)
{
    ir_instr_t *ins = calloc(1, sizeof(*ins));
    if (!ins)
        return NULL;
    ins->dest = -1;
    ins->name = NULL;
    ins->data = NULL;
    ins->is_volatile = 0;
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

/* Define a global variable named `name` with an optional initial value. */
void ir_build_glob_var(ir_builder_t *b, const char *name, long long value,
                       int is_static)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_VAR;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = value;
    ins->src1 = is_static;
}

/*
 * Define a global array `name` with the provided values.  Returns
 * non-zero on success and zero on allocation failure.
 */
int ir_build_glob_array(ir_builder_t *b, const char *name,
                        const long long *values, size_t count,
                        int is_static)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return 0;
    ins->op = IR_GLOB_ARRAY;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = (long long)count;
    ins->src1 = is_static;
    if (count) {
        long long *vals = malloc(count * sizeof(long long));
        if (!vals) {
            /* allocation failed: remove appended instruction */
            ir_instr_t *prev = NULL;
            for (ir_instr_t *cur = b->head; cur && cur != ins; cur = cur->next)
                prev = cur;
            if (prev)
                prev->next = ins->next;
            else
                b->head = ins->next;
            if (b->tail == ins)
                b->tail = prev;
            free(ins->name);
            free(ins);
            return 0;
        }
        for (size_t i = 0; i < count; i++)
            vals[i] = values[i];
        ins->data = (char *)vals;
    }
    return 1;
}

/* Begin a global union definition with the given name and size. */
void ir_build_glob_union(ir_builder_t *b, const char *name, int size,
                         int is_static)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_UNION;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = size;
    ins->src1 = is_static;
}

/* Begin a global struct definition with the given name and size. */
void ir_build_glob_struct(ir_builder_t *b, const char *name, int size,
                          int is_static)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_STRUCT;
    ins->name = vc_strdup(name ? name : "");
    ins->imm = size;
    ins->src1 = is_static;
}


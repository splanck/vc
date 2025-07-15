#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "ir_const.h"
#include "ir_builder.h"
#include "label.h"
#include "util.h"
#include "error.h"

ir_value_t ir_build_const(ir_builder_t *b, long long value)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CONST;
    ins->dest = alloc_value_id(b);
    ins->imm = value;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_cplx_const(ir_builder_t *b, double real, double imag)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CPLX_CONST;
    ins->dest = alloc_value_id(b);
    double *vals = malloc(2 * sizeof(double));
    if (!vals) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    vals[0] = real;
    vals[1] = imag;
    ins->data = (char *)vals;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_string(ir_builder_t *b, const char *str)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_GLOB_STRING;
    ins->dest = alloc_value_id(b);
    char label[32];
    const char *fmt = label_format("Lstr", ins->dest, label);
    if (!fmt) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->name = vc_strdup(fmt);
    if (!ins->name) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->data = vc_strdup(str ? str : "");
    if (!ins->data) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_wstring(ir_builder_t *b, const char *str)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_GLOB_WSTRING;
    ins->dest = alloc_value_id(b);
    size_t len = strlen(str ? str : "");
    if (len + 1 > SIZE_MAX / sizeof(long long)) {
        error_set(&error_ctx, b->cur_line, b->cur_column, b->cur_file, error_ctx.function);
        error_print(&error_ctx, "wide string literal too large");
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    long long *vals = malloc((len + 1) * sizeof(long long));
    if (!vals) {
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    for (size_t i = 0; i < len; i++)
        vals[i] = (unsigned char)str[i];
    vals[len] = 0;
    char label[32];
    const char *fmt = label_format("LWstr", ins->dest, label);
    if (!fmt) {
        free(vals);
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->name = vc_strdup(fmt);
    if (!ins->name) {
        free(vals);
        remove_instr(b, ins);
        return (ir_value_t){0};
    }
    ins->imm = (long long)(len + 1);
    ins->data = (char *)vals;
    return (ir_value_t){ins->dest};
}

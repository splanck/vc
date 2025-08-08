/*
 * Floating point helpers extracted from codegen_arith.c
 */

#include <stdio.h>
#include "codegen_arith_float.h"
#include "regalloc_x86.h"
#include "consteval.h"
#define SCRATCH_REG 0

static const char *reg_str(int reg, asm_syntax_t syntax)
{
    const char *name = regalloc_reg_name(reg);
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

static const char *fmt_reg(const char *name, asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64,
                           asm_syntax_t syntax)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return reg_str(loc, syntax);
    if (x64) {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[rbp-%d]", -loc * 8);
        else
            snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
    } else {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[ebp-%d]", -loc * 4);
        else
            snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    }
    return buf;
}

/* Convert between integer and floating-point types. */
void emit_cast(strbuf_t *sb, ir_instr_t *ins,
               regalloc_t *ra, int x64,
               asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    type_kind_t src = (type_kind_t)((unsigned long long)ins->imm >> 32);
    type_kind_t dst = (type_kind_t)(ins->imm & 0xffffffffu);
    int src64 = (src == TYPE_LLONG || src == TYPE_ULLONG);
    int dst64 = (dst == TYPE_LLONG || dst == TYPE_ULLONG);

    int r0 = regalloc_xmm_acquire();
    const char *reg0 = fmt_reg(regalloc_xmm_name(r0), syntax);
    const char *sfx = x64 ? "q" : "l";

    if (is_intlike(src) && dst == TYPE_FLOAT) {
        const char *op = src64 ? "cvtsi2ssq" : "cvtsi2ss";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", op, reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    %s %s, %s\n", op,
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        regalloc_xmm_release(r0);
        return;
    }

    if (is_intlike(src) && dst == TYPE_DOUBLE) {
        const char *op = src64 ? "cvtsi2sdq" : "cvtsi2sd";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", op, reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    %s %s, %s\n", op,
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movq %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movsd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        regalloc_xmm_release(r0);
        return;
    }

    if (src == TYPE_FLOAT && is_intlike(dst)) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        const char *op = dst64 ? "cvttss2siq" : "cvttss2si";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", op, reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    %s %s, %s\n", op, reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        regalloc_xmm_release(r0);
        return;
    }

    if (src == TYPE_DOUBLE && is_intlike(dst)) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movsd %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    movsd %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        const char *op = dst64 ? "cvttsd2siq" : "cvttsd2si";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", op, reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    %s %s, %s\n", op, reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        regalloc_xmm_release(r0);
        return;
    }

    if (src == TYPE_FLOAT && dst == TYPE_DOUBLE) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        strbuf_appendf(sb, "    cvtss2sd %s, %s\n", reg0, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movq %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movsd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        regalloc_xmm_release(r0);
        return;
    }

    if (src == TYPE_DOUBLE && dst == TYPE_FLOAT) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movsd %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    movsd %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        strbuf_appendf(sb, "    cvtsd2ss %s, %s\n", reg0, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        regalloc_xmm_release(r0);
        return;
    }

    int spill_src = ra && ra->loc[ins->src1] < 0;
    int spill_dest = ra && ra->loc[ins->dest] < 0;
    if (spill_src && spill_dest) {
        const char *tmp = reg_str(SCRATCH_REG, syntax);
        if (syntax == ASM_INTEL) {
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, tmp,
                           loc_str(b1, ra, ins->src1, x64, syntax));
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(b2, ra, ins->dest, x64, syntax), tmp);
        } else {
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(b1, ra, ins->src1, x64, syntax), tmp);
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, tmp,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        }
        regalloc_xmm_release(r0);
        return;
    }

    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
    regalloc_xmm_release(r0);
}

/* Generate a long double binary operation using the x87 FPU. */
void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins,
                           regalloc_t *ra, int x64, const char *op,
                           asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    strbuf_appendf(sb, "    fldt %s\n", loc_str(b1, ra, ins->src1, x64, syntax));
    strbuf_appendf(sb, "    fldt %s\n", loc_str(b1, ra, ins->src2, x64, syntax));
    strbuf_appendf(sb, "    %s\n", op);
    strbuf_appendf(sb, "    fstpt %s\n", loc_str(b2, ra, ins->dest, x64, syntax));
}


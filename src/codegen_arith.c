/*
 * Emitters for arithmetic IR instructions.
 *
 * Functions in this file translate high level arithmetic operations to
 * x86 assembly after register allocation.  Helpers choose the correct
 * register names and instruction suffixes depending on whether the
 * 32-bit or 64-bit backend is requested via the `x64` flag.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "codegen_arith.h"
#include "regalloc_x86.h"
#include "label.h"
#include "consteval.h"
#include "consteval.h"

#define SCRATCH_REG 0

/* Return the register name for the given allocator index. */
static const char *reg_str(int reg, asm_syntax_t syntax)
{
    const char *name = regalloc_reg_name(reg);
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

/* Format an arbitrary register name. */
static const char *fmt_reg(const char *name, asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

/* Format the location of operand `id` for assembly output. */
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

/* small helpers for the individual arithmetic ops */
/* Convert between integer and floating-point types. */
static void emit_cast(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    type_kind_t src = (type_kind_t)((unsigned long long)ins->imm >> 32);
    type_kind_t dst = (type_kind_t)(ins->imm & 0xffffffffu);

    const char *reg0 = fmt_reg("%xmm0", syntax);
    const char *sfx = x64 ? "q" : "l";

    if (is_intlike(src) && dst == TYPE_FLOAT) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    cvtsi2ss %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    cvtsi2ss %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        return;
    }

    if (is_intlike(src) && dst == TYPE_DOUBLE) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    cvtsi2sd %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    cvtsi2sd %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movq %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movsd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        return;
    }

    if (src == TYPE_FLOAT && is_intlike(dst)) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    cvttss2si %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    cvttss2si %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        return;
    }

    if (src == TYPE_DOUBLE && is_intlike(dst)) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movsd %s, %s\n", reg0,
                           loc_str(b1, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    movsd %s, %s\n",
                           loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    cvttsd2si %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    cvttsd2si %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
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
        return;
    }

    /* Default case: just move the value */
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
}
/* Add a scaled index to a pointer operand. */
static void emit_ptr_add(strbuf_t *sb, ir_instr_t *ins,
                         regalloc_t *ra, int x64,
                         asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int scale = ins->imm;
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src2, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    imul%s %s, %d\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax), scale);
    else
        strbuf_appendf(sb, "    imul%s $%d, %s\n", sfx, scale,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
}

/* Compute the difference between two pointers. */
static void emit_ptr_diff(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64,
                          asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int esz = ins->imm;
    int shift = 0;
    while ((esz >>= 1) > 0) shift++;
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    sar%s %s, %d\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax), shift);
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    sar%s $%d, %s\n", sfx, shift,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
}

/* Generate a basic float binary operation using SSE. */
static void emit_float_binop(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64, const char *op,
                             asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *reg0 = fmt_reg("%xmm0", syntax);
    const char *reg1 = fmt_reg("%xmm1", syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    movd %s, %s\n", reg1,
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    %s %s, %s\n", op, reg0, reg1);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n",
                           loc_str(b2, ra, ins->dest, x64, syntax), reg0);
        else
            strbuf_appendf(sb, "    movss %s, %s\n",
                           loc_str(b2, ra, ins->dest, x64, syntax), reg0);
    } else {
        strbuf_appendf(sb, "    movd %s, %s\n",
                       loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n",
                       loc_str(b1, ra, ins->src2, x64, syntax), reg1);
        strbuf_appendf(sb, "    %s %s, %s\n", op, reg1, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                           loc_str(b2, ra, ins->dest, x64, syntax));
    }
}

/* Generate a long double binary operation using the x87 FPU. */
static void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins,
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

/* Handle basic integer arithmetic operations. */
static void emit_int_arith(strbuf_t *sb, ir_instr_t *ins,
                           regalloc_t *ra, int x64, const char *op,
                           asm_syntax_t syntax)
{
    char b1[32];
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int dest_spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest_reg = dest_spill ? reg_str(SCRATCH_REG, syntax)
                                      : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *dest_mem = loc_str(mem, ra, ins->dest, x64, syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg,
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, dest_reg,
                       loc_str(b1, ra, ins->src2, x64, syntax));
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_mem, dest_reg);
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax), dest_reg);
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax), dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
    }
}

/* Generate code for signed integer division. */
static void emit_div(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64,
                     asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = fmt_reg(x64 ? "%rax" : "%eax", syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                       loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax), ax);
    strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
    strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64, syntax));
    if (ra && ra->loc[ins->dest] >= 0 &&
        strcmp(regalloc_reg_name(ra->loc[ins->dest]), ax) != 0)
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(b2, ra, ins->dest, x64, syntax), ax);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                           loc_str(b2, ra, ins->dest, x64, syntax));
}

/* Generate code for the modulus operation. */
static void emit_mod(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64,
                     asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = fmt_reg(x64 ? "%rax" : "%eax", syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                       loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax), ax);
    strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
    strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64, syntax));
    if (ra && ra->loc[ins->dest] >= 0 &&
        strcmp(regalloc_reg_name(ra->loc[ins->dest]),
               x64 ? "%rdx" : "%edx") != 0)
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(b2, ra, ins->dest, x64, syntax),
                           fmt_reg(x64 ? "%rdx" : "%edx", syntax));
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           fmt_reg(x64 ? "%rdx" : "%edx", syntax),
                           loc_str(b2, ra, ins->dest, x64, syntax));
}

/* Generate left or right shift instructions. */
static void emit_shift(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64, const char *op,
                       asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *cx = fmt_reg(x64 ? "%rcx" : "%ecx", syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, cx,
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, cx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax), cx);
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, "%cl",
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
}

/* Handle bitwise AND/OR/XOR operations. */
static void emit_bitwise(strbuf_t *sb, ir_instr_t *ins,
                         regalloc_t *ra, int x64, const char *op,
                         asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src2, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
}

/* Emit comparison operations producing boolean results. */
static void emit_cmp(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64,
                     asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *cc = "";
    switch (ins->op) {
    case IR_CMPEQ: cc = "e"; break;
    case IR_CMPNE: cc = "ne"; break;
    case IR_CMPLT: cc = "l"; break;
    case IR_CMPGT: cc = "g"; break;
    case IR_CMPLE: cc = "le"; break;
    case IR_CMPGE: cc = "ge"; break;
    default: break;
    }
    const char *al = fmt_reg("%al", syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    cmp%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    set%s %s\n", cc, al);
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl",
                       al, loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    cmp%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    set%s %s\n", cc, al);
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl",
                       al, loc_str(b2, ra, ins->dest, x64, syntax));
    }
}

/* Emit short-circuiting logical AND. */
static void emit_logand(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int id = label_next_id();
    char fl[32];
    char end[32];
    if (!label_format_suffix("L", id, "_false", fl) ||
        !label_format_suffix("L", id, "_end", end))
        return;
    const char *al = fmt_reg("%al", syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
    strbuf_appendf(sb, "    je %s\n", fl);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
    strbuf_appendf(sb, "    setne %s\n", al);
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", al,
                   loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    jmp %s\n", end);
    strbuf_appendf(sb, "%s:\n", fl);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, 0\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s $0, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "%s:\n", end);
}

/* Emit short-circuiting logical OR. */
static void emit_logor(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int id = label_next_id();
    char tl[32];
    char end[32];
    if (!label_format_suffix("L", id, "_true", tl) ||
        !label_format_suffix("L", id, "_end", end))
        return;
    const char *al = fmt_reg("%al", syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
    strbuf_appendf(sb, "    jne %s\n", tl);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax),
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       loc_str(b2, ra, ins->dest, x64, syntax));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    }
    strbuf_appendf(sb, "    setne %s\n", al);
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", al,
                   loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    jmp %s\n", end);
    strbuf_appendf(sb, "%s:\n", tl);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, 1\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s $1, %s\n", sfx,
                       loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "%s:\n", end);
}

/*
 * Top-level dispatcher for arithmetic instructions.
 *
 * `ra` contains the locations assigned by the register allocator and is
 * used to decide whether a result must be written back to memory.  The
 * `x64` flag selects between 32- and 64-bit instruction forms and register
 * names.
 */
void emit_arith_instr(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      asm_syntax_t syntax)
{
    switch (ins->op) {
    case IR_PTR_ADD:
        emit_ptr_add(sb, ins, ra, x64, syntax);
        break;
    case IR_PTR_DIFF:
        emit_ptr_diff(sb, ins, ra, x64, syntax);
        break;
    case IR_FADD:
        emit_float_binop(sb, ins, ra, x64, "addss", syntax);
        break;
    case IR_FSUB:
        emit_float_binop(sb, ins, ra, x64, "subss", syntax);
        break;
    case IR_FMUL:
        emit_float_binop(sb, ins, ra, x64, "mulss", syntax);
        break;
    case IR_FDIV:
        emit_float_binop(sb, ins, ra, x64, "divss", syntax);
        break;
    case IR_LFADD:
        emit_long_float_binop(sb, ins, ra, x64, "faddp", syntax);
        break;
    case IR_LFSUB:
        emit_long_float_binop(sb, ins, ra, x64, "fsubp", syntax);
        break;
    case IR_LFMUL:
        emit_long_float_binop(sb, ins, ra, x64, "fmulp", syntax);
        break;
    case IR_LFDIV:
        emit_long_float_binop(sb, ins, ra, x64, "fdivp", syntax);
        break;
    case IR_ADD:
        emit_int_arith(sb, ins, ra, x64, "add", syntax);
        break;
    case IR_SUB:
        emit_int_arith(sb, ins, ra, x64, "sub", syntax);
        break;
    case IR_MUL:
        emit_int_arith(sb, ins, ra, x64, "imul", syntax);
        break;
    case IR_DIV:
        emit_div(sb, ins, ra, x64, syntax);
        break;
    case IR_MOD:
        emit_mod(sb, ins, ra, x64, syntax);
        break;
    case IR_SHL:
        emit_shift(sb, ins, ra, x64, "sal", syntax);
        break;
    case IR_SHR:
        emit_shift(sb, ins, ra, x64, "sar", syntax);
        break;
    case IR_AND:
        emit_bitwise(sb, ins, ra, x64, "and", syntax);
        break;
    case IR_OR:
        emit_bitwise(sb, ins, ra, x64, "or", syntax);
        break;
    case IR_XOR:
        emit_bitwise(sb, ins, ra, x64, "xor", syntax);
        break;
    case IR_CAST:
        emit_cast(sb, ins, ra, x64, syntax);
        break;
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE:
        emit_cmp(sb, ins, ra, x64, syntax);
        break;
    case IR_LOGAND:
        emit_logand(sb, ins, ra, x64, syntax);
        break;
    case IR_LOGOR:
        emit_logor(sb, ins, ra, x64, syntax);
        break;
    default:
        break;
    }
}


/*
 * Emitters for arithmetic IR instructions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "codegen_arith.h"
#include "regalloc_x86.h"
#include "label.h"

#define SCRATCH_REG 0

static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return regalloc_reg_name(loc);
    if (x64)
        snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
    else
        snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    return buf;
}

void emit_arith_instr(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64)
{
    char buf1[32];
    char buf2[32];
    char destbuf[32];
    char membuf[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    int dest_spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest_reg = dest_spill ? regalloc_reg_name(SCRATCH_REG)
                                      : loc_str(destbuf, ra, ins->dest, x64);
    const char *dest_mem = loc_str(membuf, ra, ins->dest, x64);

    switch (ins->op) {
    case IR_PTR_ADD: {
        int scale = ins->imm;
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    imul%s $%d, %s\n", sfx, scale,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_PTR_DIFF: {
        int esz = ins->imm;
        int shift = 0;
        while ((esz >>= 1) > 0) shift++;
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    sar%s $%d, %s\n", sfx, shift,
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_FADD: {
        const char *reg0 = "%xmm0";
        const char *reg1 = "%xmm1";
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src1, x64), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src2, x64), reg1);
        strbuf_appendf(sb, "    addss %s, %s\n", reg1, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_FSUB: {
        const char *reg0 = "%xmm0";
        const char *reg1 = "%xmm1";
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src1, x64), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src2, x64), reg1);
        strbuf_appendf(sb, "    subss %s, %s\n", reg1, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_FMUL: {
        const char *reg0 = "%xmm0";
        const char *reg1 = "%xmm1";
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src1, x64), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src2, x64), reg1);
        strbuf_appendf(sb, "    mulss %s, %s\n", reg1, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_FDIV: {
        const char *reg0 = "%xmm0";
        const char *reg1 = "%xmm1";
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src1, x64), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n", loc_str(buf1, ra, ins->src2, x64), reg1);
        strbuf_appendf(sb, "    divss %s, %s\n", reg1, reg0);
        if (ra && ra->loc[ins->dest] >= 0)
            strbuf_appendf(sb, "    movd %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        else
            strbuf_appendf(sb, "    movss %s, %s\n", reg0, loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_LFADD:
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src1, x64));
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src2, x64));
        strbuf_append(sb, "    faddp\n");
        strbuf_appendf(sb, "    fstpt %s\n", loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_LFSUB:
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src1, x64));
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src2, x64));
        strbuf_append(sb, "    fsubp\n");
        strbuf_appendf(sb, "    fstpt %s\n", loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_LFMUL:
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src1, x64));
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src2, x64));
        strbuf_append(sb, "    fmulp\n");
        strbuf_appendf(sb, "    fstpt %s\n", loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_LFDIV:
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src1, x64));
        strbuf_appendf(sb, "    fldt %s\n", loc_str(buf1, ra, ins->src2, x64));
        strbuf_append(sb, "    fdivp\n");
        strbuf_appendf(sb, "    fstpt %s\n", loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_ADD:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), dest_reg);
        strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64), dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_SUB:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), dest_reg);
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64), dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_MUL:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), dest_reg);
        strbuf_appendf(sb, "    imul%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64), dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_DIV:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), ax);
        strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
        strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64));
        if (ra && ra->loc[ins->dest] >= 0 &&
            strcmp(regalloc_reg_name(ra->loc[ins->dest]), ax) != 0)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                           loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_MOD:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), ax);
        strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
        strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64));
        if (ra && ra->loc[ins->dest] >= 0 &&
            strcmp(regalloc_reg_name(ra->loc[ins->dest]),
                   x64 ? "%rdx" : "%edx") != 0)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           x64 ? "%rdx" : "%edx",
                           loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_SHL:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       x64 ? "%rcx" : "%ecx");
        strbuf_appendf(sb, "    sal%s %s, %s\n", sfx, "%cl",
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_SHR:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       x64 ? "%rcx" : "%ecx");
        strbuf_appendf(sb, "    sar%s %s, %s\n", sfx, "%cl",
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_AND:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    and%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_OR:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    or%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_XOR:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    xor%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE: {
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
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    set%s %s\n", cc, "%al");
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl",
                       "%al", loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_LOGAND: {
        int id = label_next_id();
        char fl[32]; char end[32];
        label_format_suffix("L", id, "_false", fl);
        label_format_suffix("L", id, "_end", end);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    je %s\n", fl);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    setne %s\n", "%al");
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", "%al",
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    jmp %s\n", end);
        strbuf_appendf(sb, "%s:\n", fl);
        strbuf_appendf(sb, "    mov%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "%s:\n", end);
        break;
    }
    case IR_LOGOR: {
        int id = label_next_id();
        char tl[32]; char end[32];
        label_format_suffix("L", id, "_true", tl);
        label_format_suffix("L", id, "_end", end);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    jne %s\n", tl);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    setne %s\n", "%al");
        strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", "%al",
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "    jmp %s\n", end);
        strbuf_appendf(sb, "%s:\n", tl);
        strbuf_appendf(sb, "    mov%s $1, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64));
        strbuf_appendf(sb, "%s:\n", end);
        break;
    }
    default:
        break;
    }
}


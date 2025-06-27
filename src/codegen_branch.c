/*
 * Emitters for branch and control-flow IR instructions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "codegen_branch.h"
#include "regalloc_x86.h"

extern int export_syms;

/* Return the register or stack location string for `id`. */
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

/* Emit jumps, calls and other branch instructions. */
void emit_branch_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64)
{
    char buf1[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sp = x64 ? "%rsp" : "%esp";

    switch (ins->op) {
    case IR_RETURN:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), ax);
        strbuf_append(sb, "    ret\n");
        break;
    case IR_CALL:
        strbuf_appendf(sb, "    call %s\n", ins->name);
        if (ins->imm > 0)
            strbuf_appendf(sb, "    add%s $%d, %s\n", sfx,
                           ins->imm * (x64 ? 8 : 4), sp);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                       loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_FUNC_BEGIN: {
        if (export_syms)
            strbuf_appendf(sb, ".globl %s\n", ins->name);
        strbuf_appendf(sb, "%s:\n", ins->name);
        strbuf_appendf(sb, "    push%s %s\n", sfx, bp);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp, bp);
        int frame = ra ? ra->stack_slots * (x64 ? 8 : 4) : 0;
        if (x64 && frame % 16 != 0)
            frame += 16 - (frame % 16);
        if (frame > 0)
            strbuf_appendf(sb, "    sub%s $%d, %s\n", sfx, frame, sp);
        break;
    }
    case IR_FUNC_END:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, bp, sp);
        strbuf_appendf(sb, "    pop%s %s\n", sfx, bp);
        strbuf_append(sb, "    ret\n");
        break;
    case IR_BR:
        strbuf_appendf(sb, "    jmp %s\n", ins->name);
        break;
    case IR_BCOND:
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64));
        strbuf_appendf(sb, "    je %s\n", ins->name);
        break;
    case IR_LABEL:
        strbuf_appendf(sb, "%s:\n", ins->name);
        break;
    case IR_ALLOCA: {
        const char *sfx = x64 ? "q" : "l";
        const char *sp = x64 ? "%rsp" : "%esp";
        char buf1[32];
        char buf2[32];
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), sp);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp,
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    default:
        break;
    }
}


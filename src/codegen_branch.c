/*
 * Emitters for branch and control-flow IR instructions.
 *
 * This module handles function prologues/epilogues, calls and jumps.  The
 * generated code uses the stack layout computed by the register allocator
 * and adapts instruction suffixes and frame sizes for 32- or 64-bit mode
 * based on the `x64` argument passed to the emitters.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "codegen_branch.h"
#include "regalloc_x86.h"


extern int export_syms;

/* Forward declarations for small helpers. */
static void emit_return(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *ax);
static void emit_call(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      const char *sfx, const char *ax, const char *sp);
static void emit_func_frame(strbuf_t *sb, ir_instr_t *ins,
                            regalloc_t *ra, int x64,
                            const char *sfx, const char *bp, const char *sp);
static void emit_jumps(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       const char *sfx);
static void emit_alloca(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *sp);

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

/* Emit a return instruction (IR_RETURN). */
static void emit_return(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *ax)
{
    char buf[32];
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf, ra, ins->src1, x64), ax);
    strbuf_append(sb, "    ret\n");
}

/* Emit a call instruction (IR_CALL). */
static void emit_call(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      const char *sfx, const char *ax, const char *sp)
{
    char buf[32];
    strbuf_appendf(sb, "    call %s\n", ins->name);
    if (ins->imm > 0)
        strbuf_appendf(sb, "    add%s $%d, %s\n", sfx,
                       ins->imm * (x64 ? 8 : 4), sp);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                   loc_str(buf, ra, ins->dest, x64));
}

/* Emit an indirect call instruction (IR_CALL_PTR). */
static void emit_call_ptr(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64,
                          const char *sfx, const char *ax, const char *sp)
{
    char buf[32];
    strbuf_appendf(sb, "    call *%s\n", loc_str(buf, ra, ins->src1, x64));
    if (ins->imm > 0)
        strbuf_appendf(sb, "    add%s $%d, %s\n", sfx,
                       ins->imm * (x64 ? 8 : 4), sp);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                   loc_str(buf, ra, ins->dest, x64));
}

/* Emit function prologue and epilogue. */
static void emit_func_frame(strbuf_t *sb, ir_instr_t *ins,
                            regalloc_t *ra, int x64,
                            const char *sfx, const char *bp, const char *sp)
{
    if (ins->op == IR_FUNC_BEGIN) {
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
    } else { /* IR_FUNC_END */
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, bp, sp);
        strbuf_appendf(sb, "    pop%s %s\n", sfx, bp);
        strbuf_append(sb, "    ret\n");
    }
}

/* Emit unconditional and conditional jumps. */
static void emit_jumps(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       const char *sfx)
{
    char buf[32];
    if (ins->op == IR_BR) {
        strbuf_appendf(sb, "    jmp %s\n", ins->name);
    } else if (ins->op == IR_BCOND) {
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       loc_str(buf, ra, ins->src1, x64));
        strbuf_appendf(sb, "    je %s\n", ins->name);
    } else if (ins->op == IR_LABEL) {
        strbuf_appendf(sb, "%s:\n", ins->name);
    }
}

/* Emit stack allocation instruction (IR_ALLOCA). */
static void emit_alloca(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *sp)
{
    char buf1[32];
    char buf2[32];
    strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64), sp);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp,
                   loc_str(buf2, ra, ins->dest, x64));
}

/*
 * Emit jumps, calls and other branch instructions.
 *
 * The register allocator provides operand locations and frame size.  This
 * function uses that information to generate correct prologue/epilogue
 * code and to lower control-flow opcodes.  Instruction variants differ
 * between 32- and 64-bit mode; the `x64` flag selects the proper suffixes
 * and stack pointer registers.
 */
void emit_branch_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax)
{
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sp = x64 ? "%rsp" : "%esp";

    switch (ins->op) {
    case IR_RETURN:
        emit_return(sb, ins, ra, x64, sfx, ax);
        break;
    case IR_CALL:
        emit_call(sb, ins, ra, x64, sfx, ax, sp);
        break;
    case IR_CALL_PTR:
        emit_call_ptr(sb, ins, ra, x64, sfx, ax, sp);
        break;
    case IR_FUNC_BEGIN: case IR_FUNC_END:
        emit_func_frame(sb, ins, ra, x64, sfx, bp, sp);
        break;
    case IR_BR: case IR_BCOND: case IR_LABEL:
        emit_jumps(sb, ins, ra, x64, sfx);
        break;
    case IR_ALLOCA:
        emit_alloca(sb, ins, ra, x64, sfx, sp);
        break;
    default:
        break;
    }
}


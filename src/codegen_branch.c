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
extern size_t arg_stack_bytes;
extern int dwarf_enabled;

/* Forward declarations for small helpers. */
static void emit_return(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *ax,
                        asm_syntax_t syntax);
static void emit_call(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      const char *sfx, const char *ax, const char *sp,
                      asm_syntax_t syntax);
static void emit_func_frame(strbuf_t *sb, ir_instr_t *ins,
                            regalloc_t *ra, int x64,
                            const char *sfx, const char *bp, const char *sp,
                            asm_syntax_t syntax);
static void emit_jumps(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       const char *sfx, asm_syntax_t syntax);
static void emit_alloca(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *sp,
                        asm_syntax_t syntax);

/* Return the register or stack location string for `id`. */
/* Format a register name for the requested syntax. */
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

/* Helper returning the textual location of operand `id`. */
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

/* Emit a return instruction (IR_RETURN or IR_RETURN_AGG). */
static void emit_return(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *ax,
                        asm_syntax_t syntax)
{
    char buf[32];
    if (ins->op == IR_RETURN) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                           loc_str(buf, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(buf, ra, ins->src1, x64, syntax), ax);
    } else { /* IR_RETURN_AGG */
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s [%s], %s\n", sfx, ax,
                           loc_str(buf, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    mov%s %s, (%s)\n", sfx,
                           loc_str(buf, ra, ins->src1, x64, syntax), ax);
    }
    strbuf_append(sb, "    ret\n");
}

/* Emit a call instruction (IR_CALL). */
static void emit_call(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      const char *sfx, const char *ax, const char *sp,
                      asm_syntax_t syntax)
{
    char buf[32];
    strbuf_appendf(sb, "    call %s\n", ins->name);
    if (ins->imm > 0 && arg_stack_bytes > 0) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    add%s %s, %zu\n", sfx, sp,
                           arg_stack_bytes);
        else
            strbuf_appendf(sb, "    add%s $%zu, %s\n", sfx,
                           arg_stack_bytes, sp);
    }
    arg_stack_bytes = 0;
    if (ins->dest > 0) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(buf, ra, ins->dest, x64, syntax), ax);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                           loc_str(buf, ra, ins->dest, x64, syntax));
    }
}

/* Emit an indirect call instruction (IR_CALL_PTR). */
static void emit_call_ptr(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64,
                          const char *sfx, const char *ax, const char *sp,
                          asm_syntax_t syntax)
{
    char buf[32];
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    call %s\n", loc_str(buf, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    call *%s\n", loc_str(buf, ra, ins->src1, x64, syntax));
    if (ins->imm > 0 && arg_stack_bytes > 0) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    add%s %s, %zu\n", sfx, sp,
                           arg_stack_bytes);
        else
            strbuf_appendf(sb, "    add%s $%zu, %s\n", sfx,
                           arg_stack_bytes, sp);
    }
    arg_stack_bytes = 0;
    if (ins->dest > 0) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                           loc_str(buf, ra, ins->dest, x64, syntax), ax);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                           loc_str(buf, ra, ins->dest, x64, syntax));
    }
}

/* Emit function prologue and epilogue. */
static void emit_func_frame(strbuf_t *sb, ir_instr_t *ins,
                            regalloc_t *ra, int x64,
                            const char *sfx, const char *bp, const char *sp,
                            asm_syntax_t syntax)
{
    static const char *cur_func = NULL;
    if (ins->op == IR_FUNC_BEGIN) {
        if (export_syms)
            strbuf_appendf(sb, ".globl %s\n", ins->name);
        if (dwarf_enabled)
            strbuf_appendf(sb, ".type %s, @function\n", ins->name);
        strbuf_appendf(sb, "%s:\n", ins->name);
        strbuf_appendf(sb, "    push%s %s\n", sfx, bp);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, bp, sp);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp, bp);
        int frame = ra ? ra->stack_slots * (x64 ? 8 : 4) : 0;
        if (x64 && frame % 16 != 0)
            frame += 16 - (frame % 16);
        if (frame > 0) {
            if (syntax == ASM_INTEL)
                strbuf_appendf(sb, "    sub%s %s, %d\n", sfx, sp, frame);
            else
                strbuf_appendf(sb, "    sub%s $%d, %s\n", sfx, frame, sp);
        }
        cur_func = ins->name;
    } else { /* IR_FUNC_END */
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp, bp);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, bp, sp);
        strbuf_appendf(sb, "    pop%s %s\n", sfx, bp);
        strbuf_append(sb, "    ret\n");
        if (dwarf_enabled && cur_func)
            strbuf_appendf(sb, ".size %s, .-%s\n", cur_func, cur_func);
    }
}

/* Emit unconditional and conditional jumps. */
static void emit_jumps(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       const char *sfx, asm_syntax_t syntax)
{
    char buf[32];
    if (ins->op == IR_BR) {
        strbuf_appendf(sb, "    jmp %s\n", ins->name);
    } else if (ins->op == IR_BCOND) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                           loc_str(buf, ra, ins->src1, x64, syntax));
        else
            strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                           loc_str(buf, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    je %s\n", ins->name);
    } else if (ins->op == IR_LABEL) {
        strbuf_appendf(sb, "%s:\n", ins->name);
    }
}

/* Emit stack allocation instruction (IR_ALLOCA). */
static void emit_alloca(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        const char *sfx, const char *sp,
                        asm_syntax_t syntax)
{
    char buf1[32];
    char buf2[32];
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx, sp,
                       loc_str(buf1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf2, ra, ins->dest, x64, syntax), sp);
    } else {
        strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64, syntax), sp);
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, sp,
                       loc_str(buf2, ra, ins->dest, x64, syntax));
    }
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
    const char *ax = fmt_reg(x64 ? "%rax" : "%eax", syntax);
    const char *bp = fmt_reg(x64 ? "%rbp" : "%ebp", syntax);
    const char *sp = fmt_reg(x64 ? "%rsp" : "%esp", syntax);

    switch (ins->op) {
    case IR_RETURN:
    case IR_RETURN_AGG:
        emit_return(sb, ins, ra, x64, sfx, ax, syntax);
        break;
    case IR_CALL:
    case IR_CALL_NR:
        emit_call(sb, ins, ra, x64, sfx, ax, sp, syntax);
        break;
    case IR_CALL_PTR:
    case IR_CALL_PTR_NR:
        emit_call_ptr(sb, ins, ra, x64, sfx, ax, sp, syntax);
        break;
    case IR_FUNC_BEGIN: case IR_FUNC_END:
        emit_func_frame(sb, ins, ra, x64, sfx, bp, sp, syntax);
        break;
    case IR_BR: case IR_BCOND: case IR_LABEL:
        emit_jumps(sb, ins, ra, x64, sfx, syntax);
        break;
    case IR_ALLOCA:
        emit_alloca(sb, ins, ra, x64, sfx, sp, syntax);
        break;
    default:
        break;
    }
}


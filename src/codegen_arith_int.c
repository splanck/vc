/*
 * Integer arithmetic helpers extracted from codegen_arith.c
 */

#include <stdio.h>
#include <string.h>
#include "codegen_arith.h"
#include "codegen_arith_int.h"
#include "codegen_arith_float.h"
#include "codegen_float.h"
#include "codegen_x86.h"
#include "label.h"

#define SCRATCH_REG 0

void emit_ptr_add(strbuf_t *sb, ir_instr_t *ins,
                  regalloc_t *ra, int x64,
                  asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int scale = ins->imm;
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src2, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax),
                 syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    imul%s %s, %d\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax), scale);
    else
        strbuf_appendf(sb, "    imul%s $%d, %s\n", sfx, scale,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    x86_emit_op(sb, "add", sfx,
                x86_loc_str(b1, ra, ins->src1, x64, syntax),
                x86_loc_str(b2, ra, ins->dest, x64, syntax),
                syntax);
}

void emit_ptr_diff(strbuf_t *sb, ir_instr_t *ins,
                   regalloc_t *ra, int x64,
                   asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int esz = ins->imm;
    int shift = 0;
    while ((esz >>= 1) > 0) shift++;
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    x86_emit_op(sb, "sub", sfx,
                x86_loc_str(b1, ra, ins->src2, x64, syntax),
                x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    sar%s %s, %d\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax), shift);
    else
        strbuf_appendf(sb, "    sar%s $%d, %s\n", sfx, shift,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
}

void emit_int_arith(strbuf_t *sb, ir_instr_t *ins,
                    regalloc_t *ra, int x64, const char *op,
                    asm_syntax_t syntax)
{
    char b1[32];
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int dest_spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest_reg = dest_spill ? x86_reg_str(SCRATCH_REG, syntax)
                                      : x86_loc_str(destb, ra, ins->dest, x64, syntax);
    const char *dest_mem = x86_loc_str(mem, ra, ins->dest, x64, syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax), dest_reg, syntax);
    x86_emit_op(sb, op, sfx,
                x86_loc_str(b1, ra, ins->src2, x64, syntax), dest_reg, syntax);
    if (dest_spill)
        x86_emit_mov(sb, sfx, dest_reg, dest_mem, syntax);
}

void emit_div(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x86_fmt_reg(x64 ? "%rax" : "%eax", syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax), ax,
                 syntax);
    strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
    strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                   x86_loc_str(b1, ra, ins->src2, x64, syntax));
    if (ra && ra->loc[ins->dest] >= 0 &&
        strcmp(regalloc_reg_name(ra->loc[ins->dest]), ax) != 0) {
        x86_emit_mov(sb, sfx, ax,
                     x86_loc_str(b2, ra, ins->dest, x64, syntax),
                     syntax);
    }
}

void emit_mod(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x86_fmt_reg(x64 ? "%rax" : "%eax", syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax), ax,
                 syntax);
    strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
    strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                   x86_loc_str(b1, ra, ins->src2, x64, syntax));
    if (ra && ra->loc[ins->dest] >= 0 &&
        strcmp(regalloc_reg_name(ra->loc[ins->dest]),
               x64 ? "%rdx" : "%edx") != 0) {
        x86_emit_mov(sb, sfx,
                     x86_fmt_reg(x64 ? "%rdx" : "%edx", syntax),
                     x86_loc_str(b2, ra, ins->dest, x64, syntax),
                     syntax);
    }
}

void emit_shift(strbuf_t *sb, ir_instr_t *ins,
                regalloc_t *ra, int x64, const char *op,
                asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *cx = x86_fmt_reg(x64 ? "%rcx" : "%ecx", syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src2, x64, syntax), cx, syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, cx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, "%cl",
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
}

void emit_bitwise(strbuf_t *sb, ir_instr_t *ins,
                  regalloc_t *ra, int x64, const char *op,
                  asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    x86_emit_op(sb, op, sfx,
                x86_loc_str(b1, ra, ins->src2, x64, syntax),
                x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
}

void emit_cmp(strbuf_t *sb, ir_instr_t *ins,
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
    const char *al = x86_fmt_reg("%al", syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    strbuf_appendf(sb, "    cmp%s %s, %s\n", sfx,
                   x86_loc_str(b1, ra, ins->src2, x64, syntax),
                   x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    set%s %s\n", cc, al);
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", al,
                   x86_loc_str(b2, ra, ins->dest, x64, syntax));
}

void emit_logand(strbuf_t *sb, ir_instr_t *ins,
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
    const char *al = x86_fmt_reg("%al", syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    je %s\n", fl);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src2, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    setne %s\n", al);
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", al,
                   x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    jmp %s\n", end);
    strbuf_appendf(sb, "%s:\n", fl);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, 0\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s $0, %s\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "%s:\n", end);
}

void emit_logor(strbuf_t *sb, ir_instr_t *ins,
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
    const char *al = x86_fmt_reg("%al", syntax);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src1, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    jne %s\n", tl);
    x86_emit_mov(sb, sfx,
                 x86_loc_str(b1, ra, ins->src2, x64, syntax),
                 x86_loc_str(b2, ra, ins->dest, x64, syntax), syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    cmp%s %s, 0\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    setne %s\n", al);
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", al,
                   x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "    jmp %s\n", end);
    strbuf_appendf(sb, "%s:\n", tl);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, 1\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s $1, %s\n", sfx,
                       x86_loc_str(b2, ra, ins->dest, x64, syntax));
    strbuf_appendf(sb, "%s:\n", end);
}

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
    case IR_CPLX_ADD:
        emit_cplx_addsub(sb, ins, ra, x64, "addsd", syntax);
        break;
    case IR_CPLX_SUB:
        emit_cplx_addsub(sb, ins, ra, x64, "subsd", syntax);
        break;
    case IR_CPLX_MUL:
        emit_cplx_mul(sb, ins, ra, x64, syntax);
        break;
    case IR_CPLX_DIV:
        emit_cplx_div(sb, ins, ra, x64, syntax);
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


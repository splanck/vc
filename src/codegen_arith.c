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

#define SCRATCH_REG 0

/* Format the location of operand `id` for assembly output. */
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

/* small helpers for the individual arithmetic ops */
/* Add a scaled index to a pointer operand. */
static void emit_ptr_add(strbuf_t *sb, ir_instr_t *ins,
                         regalloc_t *ra, int x64)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int scale = ins->imm;
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    imul%s $%d, %s\n", sfx, scale,
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    add%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
}

/* Compute the difference between two pointers. */
static void emit_ptr_diff(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    int esz = ins->imm;
    int shift = 0;
    while ((esz >>= 1) > 0) shift++;
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    sub%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    sar%s $%d, %s\n", sfx, shift,
                   loc_str(b2, ra, ins->dest, x64));
}

/* Generate a basic float binary operation using SSE. */
static void emit_float_binop(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64, const char *op)
{
    char b1[32];
    char b2[32];
    const char *reg0 = "%xmm0";
    const char *reg1 = "%xmm1";
    strbuf_appendf(sb, "    movd %s, %s\n", loc_str(b1, ra, ins->src1, x64), reg0);
    strbuf_appendf(sb, "    movd %s, %s\n", loc_str(b1, ra, ins->src2, x64), reg1);
    strbuf_appendf(sb, "    %s %s, %s\n", op, reg1, reg0);
    if (ra && ra->loc[ins->dest] >= 0)
        strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                       loc_str(b2, ra, ins->dest, x64));
    else
        strbuf_appendf(sb, "    movss %s, %s\n", reg0,
                       loc_str(b2, ra, ins->dest, x64));
}

/* Generate a long double binary operation using the x87 FPU. */
static void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins,
                                  regalloc_t *ra, int x64, const char *op)
{
    char b1[32];
    char b2[32];
    strbuf_appendf(sb, "    fldt %s\n", loc_str(b1, ra, ins->src1, x64));
    strbuf_appendf(sb, "    fldt %s\n", loc_str(b1, ra, ins->src2, x64));
    strbuf_appendf(sb, "    %s\n", op);
    strbuf_appendf(sb, "    fstpt %s\n", loc_str(b2, ra, ins->dest, x64));
}

/* Handle basic integer arithmetic operations. */
static void emit_int_arith(strbuf_t *sb, ir_instr_t *ins,
                           regalloc_t *ra, int x64, const char *op)
{
    char b1[32];
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int dest_spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest_reg = dest_spill ? regalloc_reg_name(SCRATCH_REG)
                                      : loc_str(destb, ra, ins->dest, x64);
    const char *dest_mem = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64), dest_reg);
    strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx,
                   loc_str(b1, ra, ins->src2, x64), dest_reg);
    if (dest_spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
}

/* Generate code for signed integer division. */
static void emit_div(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64), ax);
    strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
    strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64));
    if (ra && ra->loc[ins->dest] >= 0 &&
        strcmp(regalloc_reg_name(ra->loc[ins->dest]), ax) != 0)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                       loc_str(b2, ra, ins->dest, x64));
}

/* Generate code for the modulus operation. */
static void emit_mod(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64), ax);
    strbuf_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
    strbuf_appendf(sb, "    idiv%s %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64));
    if (ra && ra->loc[ins->dest] >= 0 &&
        strcmp(regalloc_reg_name(ra->loc[ins->dest]),
               x64 ? "%rdx" : "%edx") != 0)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       x64 ? "%rdx" : "%edx",
                       loc_str(b2, ra, ins->dest, x64));
}

/* Generate left or right shift instructions. */
static void emit_shift(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64, const char *op)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   x64 ? "%rcx" : "%ecx");
    strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx, "%cl",
                   loc_str(b2, ra, ins->dest, x64));
}

/* Handle bitwise AND/OR/XOR operations. */
static void emit_bitwise(strbuf_t *sb, ir_instr_t *ins,
                         regalloc_t *ra, int x64, const char *op)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    %s%s %s, %s\n", op, sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->dest, x64));
}

/* Emit comparison operations producing boolean results. */
static void emit_cmp(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64)
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
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    cmp%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    set%s %s\n", cc, "%al");
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl",
                   "%al", loc_str(b2, ra, ins->dest, x64));
}

/* Emit short-circuiting logical AND. */
static void emit_logand(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64)
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
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    je %s\n", fl);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    setne %s\n", "%al");
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", "%al",
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    jmp %s\n", end);
    strbuf_appendf(sb, "%s:\n", fl);
    strbuf_appendf(sb, "    mov%s $0, %s\n", sfx,
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "%s:\n", end);
}

/* Emit short-circuiting logical OR. */
static void emit_logor(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64)
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
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    jne %s\n", tl);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    cmp%s $0, %s\n", sfx,
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    setne %s\n", "%al");
    strbuf_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl", "%al",
                   loc_str(b2, ra, ins->dest, x64));
    strbuf_appendf(sb, "    jmp %s\n", end);
    strbuf_appendf(sb, "%s:\n", tl);
    strbuf_appendf(sb, "    mov%s $1, %s\n", sfx,
                   loc_str(b2, ra, ins->dest, x64));
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
                      regalloc_t *ra, int x64)
{
    switch (ins->op) {
    case IR_PTR_ADD:
        emit_ptr_add(sb, ins, ra, x64);
        break;
    case IR_PTR_DIFF:
        emit_ptr_diff(sb, ins, ra, x64);
        break;
    case IR_FADD:
        emit_float_binop(sb, ins, ra, x64, "addss");
        break;
    case IR_FSUB:
        emit_float_binop(sb, ins, ra, x64, "subss");
        break;
    case IR_FMUL:
        emit_float_binop(sb, ins, ra, x64, "mulss");
        break;
    case IR_FDIV:
        emit_float_binop(sb, ins, ra, x64, "divss");
        break;
    case IR_LFADD:
        emit_long_float_binop(sb, ins, ra, x64, "faddp");
        break;
    case IR_LFSUB:
        emit_long_float_binop(sb, ins, ra, x64, "fsubp");
        break;
    case IR_LFMUL:
        emit_long_float_binop(sb, ins, ra, x64, "fmulp");
        break;
    case IR_LFDIV:
        emit_long_float_binop(sb, ins, ra, x64, "fdivp");
        break;
    case IR_ADD:
        emit_int_arith(sb, ins, ra, x64, "add");
        break;
    case IR_SUB:
        emit_int_arith(sb, ins, ra, x64, "sub");
        break;
    case IR_MUL:
        emit_int_arith(sb, ins, ra, x64, "imul");
        break;
    case IR_DIV:
        emit_div(sb, ins, ra, x64);
        break;
    case IR_MOD:
        emit_mod(sb, ins, ra, x64);
        break;
    case IR_SHL:
        emit_shift(sb, ins, ra, x64, "sal");
        break;
    case IR_SHR:
        emit_shift(sb, ins, ra, x64, "sar");
        break;
    case IR_AND:
        emit_bitwise(sb, ins, ra, x64, "and");
        break;
    case IR_OR:
        emit_bitwise(sb, ins, ra, x64, "or");
        break;
    case IR_XOR:
        emit_bitwise(sb, ins, ra, x64, "xor");
        break;
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE:
        emit_cmp(sb, ins, ra, x64);
        break;
    case IR_LOGAND:
        emit_logand(sb, ins, ra, x64);
        break;
    case IR_LOGOR:
        emit_logor(sb, ins, ra, x64);
        break;
    default:
        break;
    }
}


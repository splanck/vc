#include <stdio.h>
#include "codegen_float.h"
#include "regalloc_x86.h"
#include "codegen_x86.h"

/* Helper functions duplicated from codegen_arith.c */
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

static int acquire_xmm_temp(strbuf_t *sb, asm_syntax_t syntax, int *spilled)
{
    int r = regalloc_xmm_acquire();
    if (r >= 0) {
        *spilled = 0;
        return r;
    }
    r = 0;
    const char *name = fmt_reg(regalloc_xmm_name(r), syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    sub rsp, 16\n    movaps %s, [rsp]\n", name);
    else
        strbuf_appendf(sb, "    sub $16, %rsp\n    movaps %s, (%rsp)\n", name);
    *spilled = 1;
    return r;
}

static void release_xmm_temp(strbuf_t *sb, asm_syntax_t syntax,
                             int reg, int spilled)
{
    const char *name = fmt_reg(regalloc_xmm_name(reg), syntax);
    if (spilled) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movaps [rsp], %s\n    add rsp, 16\n", name);
        else
            strbuf_appendf(sb, "    movaps (%rsp), %s\n    add $16, %rsp\n", name);
    } else {
        regalloc_xmm_release(reg);
    }
}

/* Compute the location of the imaginary or real part of a complex value. */
static const char *loc_str_off(char buf[32], regalloc_t *ra, int id,
                               int off, int x64, asm_syntax_t syntax)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return reg_str(loc, syntax);
    int size = x64 ? 8 : 4;
    int disp = -loc * size + off;
    if (syntax == ASM_INTEL)
        snprintf(buf, 32, x64 ? "[rbp-%d]" : "[ebp-%d]", disp);
    else
        snprintf(buf, 32, "-%d(%s)", disp, x64 ? "%rbp" : "%ebp");
    return buf;
}

/* Emit an SSE2 move or operation with automatic syntax selection. */
static void emit_movsd(strbuf_t *sb, const char *src, const char *dest,
                       asm_syntax_t syntax)
{
    x86_emit_mov(sb, "sd", src, dest, syntax);
}

static void emit_op_sd(strbuf_t *sb, const char *op,
                       const char *src, const char *dest,
                       asm_syntax_t syntax)
{
    x86_emit_op(sb, op, "sd", src, dest, syntax);
}

/* Complex add/sub helper using SSE2. */
void emit_cplx_addsub(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                      int x64, const char *op, asm_syntax_t syntax)
{
    char b1[32];
    char b2[32];
    char b3[32];
    int spill0 = 0;
    int r0 = acquire_xmm_temp(sb, syntax, &spill0);
    int spill1 = 0;
    int r1 = acquire_xmm_temp(sb, syntax, &spill1);
    const char *reg0 = fmt_reg(regalloc_xmm_name(r0), syntax);
    const char *reg1 = fmt_reg(regalloc_xmm_name(r1), syntax);

    /* real part */
    emit_movsd(sb, loc_str_off(b1, ra, ins->src1, 0, x64, syntax), reg0,
               syntax);
    emit_movsd(sb, loc_str_off(b2, ra, ins->src2, 0, x64, syntax), reg1,
               syntax);
    emit_op_sd(sb, op, reg1, reg0, syntax);
    emit_movsd(sb, reg0, loc_str_off(b3, ra, ins->dest, 0, x64, syntax),
               syntax);

    /* imag part */
    emit_movsd(sb, loc_str_off(b1, ra, ins->src1, 8, x64, syntax), reg0,
               syntax);
    emit_movsd(sb, loc_str_off(b2, ra, ins->src2, 8, x64, syntax), reg1,
               syntax);
    emit_op_sd(sb, op, reg1, reg0, syntax);
    emit_movsd(sb, reg0, loc_str_off(b3, ra, ins->dest, 8, x64, syntax),
               syntax);
    release_xmm_temp(sb, syntax, r1, spill1);
    release_xmm_temp(sb, syntax, r0, spill0);
}

/* Complex multiplication using SSE2. */
void emit_cplx_mul(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                   int x64, asm_syntax_t syntax)
{
    char a[32], c[32], d[32], out[32];
    int s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    int r0 = acquire_xmm_temp(sb, syntax, &s0);
    int r1 = acquire_xmm_temp(sb, syntax, &s1);
    int r2 = acquire_xmm_temp(sb, syntax, &s2);
    int r3 = acquire_xmm_temp(sb, syntax, &s3);
    const char *x0 = fmt_reg(regalloc_xmm_name(r0), syntax);
    const char *x1 = fmt_reg(regalloc_xmm_name(r1), syntax);
    const char *x2 = fmt_reg(regalloc_xmm_name(r2), syntax);
    const char *x3 = fmt_reg(regalloc_xmm_name(r3), syntax);

    /* real part: (a*c - b*d) */
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 0, x64, syntax), x0, syntax);
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 8, x64, syntax), x1, syntax);
    emit_movsd(sb, loc_str_off(c, ra, ins->src2, 0, x64, syntax), x2, syntax);
    emit_movsd(sb, loc_str_off(d, ra, ins->src2, 8, x64, syntax), x3, syntax);
    emit_op_sd(sb, "mul", x2, x0, syntax);
    emit_op_sd(sb, "mul", x3, x1, syntax);
    emit_op_sd(sb, "sub", x1, x0, syntax);
    emit_movsd(sb, x0, loc_str_off(out, ra, ins->dest, 0, x64, syntax), syntax);

    /* imag part: (a*d + b*c) */
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 0, x64, syntax), x0, syntax);
    emit_op_sd(sb, "mul", x3, x0, syntax);
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 8, x64, syntax), x1, syntax);
    emit_op_sd(sb, "mul", x2, x1, syntax);
    emit_op_sd(sb, "add", x1, x0, syntax);
    emit_movsd(sb, x0, loc_str_off(out, ra, ins->dest, 8, x64, syntax), syntax);
    release_xmm_temp(sb, syntax, r3, s3);
    release_xmm_temp(sb, syntax, r2, s2);
    release_xmm_temp(sb, syntax, r1, s1);
    release_xmm_temp(sb, syntax, r0, s0);
}

/* Complex division using SSE2. */
void emit_cplx_div(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                   int x64, asm_syntax_t syntax)
{
    char a[32], c[32], d[32], out[32];
    int s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    int r0 = acquire_xmm_temp(sb, syntax, &s0);
    int r1 = acquire_xmm_temp(sb, syntax, &s1);
    int r2 = acquire_xmm_temp(sb, syntax, &s2);
    int r3 = acquire_xmm_temp(sb, syntax, &s3);
    int r4 = acquire_xmm_temp(sb, syntax, &s4);
    const char *x0 = fmt_reg(regalloc_xmm_name(r0), syntax);
    const char *x1 = fmt_reg(regalloc_xmm_name(r1), syntax);
    const char *x2 = fmt_reg(regalloc_xmm_name(r2), syntax);
    const char *x3 = fmt_reg(regalloc_xmm_name(r3), syntax);
    const char *x4 = fmt_reg(regalloc_xmm_name(r4), syntax);

    /* denominator c*c + d*d */
    emit_movsd(sb, loc_str_off(c, ra, ins->src2, 0, x64, syntax), x2, syntax);
    emit_movsd(sb, loc_str_off(d, ra, ins->src2, 8, x64, syntax), x3, syntax);
    emit_movsd(sb, x2, x4, syntax);
    emit_op_sd(sb, "mul", x2, x2, syntax);
    emit_op_sd(sb, "mul", x3, x3, syntax);
    emit_op_sd(sb, "add", x3, x2, syntax);

    /* real part */
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 0, x64, syntax), x0, syntax);
    emit_op_sd(sb, "mul", x4, x0, syntax);
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 8, x64, syntax), x1, syntax);
    emit_movsd(sb, loc_str_off(d, ra, ins->src2, 8, x64, syntax), x3, syntax);
    emit_op_sd(sb, "mul", x3, x1, syntax);
    emit_op_sd(sb, "add", x1, x0, syntax);
    emit_op_sd(sb, "div", x2, x0, syntax);
    emit_movsd(sb, x0, loc_str_off(out, ra, ins->dest, 0, x64, syntax), syntax);

    /* imag part */
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 8, x64, syntax), x0, syntax);
    emit_op_sd(sb, "mul", x4, x0, syntax);
    emit_movsd(sb, loc_str_off(a, ra, ins->src1, 0, x64, syntax), x1, syntax);
    emit_movsd(sb, loc_str_off(d, ra, ins->src2, 8, x64, syntax), x3, syntax);
    emit_op_sd(sb, "mul", x3, x1, syntax);
    emit_op_sd(sb, "sub", x1, x0, syntax);
    emit_op_sd(sb, "div", x2, x0, syntax);
    emit_movsd(sb, x0, loc_str_off(out, ra, ins->dest, 8, x64, syntax), syntax);

    release_xmm_temp(sb, syntax, r4, s4);
    release_xmm_temp(sb, syntax, r3, s3);
    release_xmm_temp(sb, syntax, r2, s2);
    release_xmm_temp(sb, syntax, r1, s1);
    release_xmm_temp(sb, syntax, r0, s0);
}

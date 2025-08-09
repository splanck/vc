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
    int r0 = regalloc_xmm_acquire();
    if (r0 < 0) {
        fprintf(stderr, "emit_cplx_addsub: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r1 = regalloc_xmm_acquire();
    if (r1 < 0) {
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_addsub: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
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
    regalloc_xmm_release(r1);
    regalloc_xmm_release(r0);
}

/* Complex multiplication using SSE2. */
void emit_cplx_mul(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                   int x64, asm_syntax_t syntax)
{
    char a[32], c[32], d[32], out[32];
    int r0 = regalloc_xmm_acquire();
    if (r0 < 0) {
        fprintf(stderr, "emit_cplx_mul: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r1 = regalloc_xmm_acquire();
    if (r1 < 0) {
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_mul: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r2 = regalloc_xmm_acquire();
    if (r2 < 0) {
        regalloc_xmm_release(r1);
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_mul: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r3 = regalloc_xmm_acquire();
    if (r3 < 0) {
        regalloc_xmm_release(r2);
        regalloc_xmm_release(r1);
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_mul: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
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
    regalloc_xmm_release(r3);
    regalloc_xmm_release(r2);
    regalloc_xmm_release(r1);
    regalloc_xmm_release(r0);
}

/* Complex division using SSE2. */
void emit_cplx_div(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                   int x64, asm_syntax_t syntax)
{
    char a[32], c[32], d[32], out[32];
    int r0 = regalloc_xmm_acquire();
    if (r0 < 0) {
        fprintf(stderr, "emit_cplx_div: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r1 = regalloc_xmm_acquire();
    if (r1 < 0) {
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_div: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r2 = regalloc_xmm_acquire();
    if (r2 < 0) {
        regalloc_xmm_release(r1);
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_div: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r3 = regalloc_xmm_acquire();
    if (r3 < 0) {
        regalloc_xmm_release(r2);
        regalloc_xmm_release(r1);
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_div: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
    int r4 = regalloc_xmm_acquire();
    if (r4 < 0) {
        regalloc_xmm_release(r3);
        regalloc_xmm_release(r2);
        regalloc_xmm_release(r1);
        regalloc_xmm_release(r0);
        fprintf(stderr, "emit_cplx_div: XMM register allocation failed\n");
        strbuf_appendf(sb, "    # XMM register allocation failed\n");
        return;
    }
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

    regalloc_xmm_release(r4);
    regalloc_xmm_release(r3);
    regalloc_xmm_release(r2);
    regalloc_xmm_release(r1);
    regalloc_xmm_release(r0);
}

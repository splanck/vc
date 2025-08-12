#include <stdio.h>
#include "codegen_float.h"
#include "regalloc_x86.h"

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

static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64,
                           asm_syntax_t syntax)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return reg_str(loc, syntax);
    int n;
    if (x64) {
        if (syntax == ASM_INTEL) {
            n = snprintf(buf, 32, "[rbp-%d]", -loc * 8);
            if (n < 0 || n >= 32)
                return "";
        } else {
            n = snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
            if (n < 0 || n >= 32)
                return "";
        }
    } else {
        if (syntax == ASM_INTEL) {
            n = snprintf(buf, 32, "[ebp-%d]", -loc * 4);
            if (n < 0 || n >= 32)
                return "";
        } else {
            n = snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
            if (n < 0 || n >= 32)
                return "";
        }
    }
    return buf;
}

/* Generate a basic float binary operation using SSE. */
void emit_float_binop(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64, const char *op,
                      asm_syntax_t syntax)
{
    char b1[32];
    int spill0 = 0;
    int r0 = acquire_xmm_temp(sb, syntax, &spill0);
    int spill1 = 0;
    int r1 = acquire_xmm_temp(sb, syntax, &spill1);
    const char *reg0 = fmt_reg(regalloc_xmm_name(r0), syntax);
    const char *reg1 = fmt_reg(regalloc_xmm_name(r1), syntax);

    /* In Intel syntax the first operand is both the left-hand side and
       the destination of the arithmetic operation.  Record the result
       register explicitly to avoid confusion when storing the final
       value. */
    const char *result = (syntax == ASM_INTEL) ? reg0 : reg1;

    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    movd %s, %s\n", reg1,
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    %s %s, %s\n", op, reg0, reg1);
    } else {
        strbuf_appendf(sb, "    movd %s, %s\n",
                       loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n",
                       loc_str(b1, ra, ins->src2, x64, syntax), reg1);
        strbuf_appendf(sb, "    %s %s, %s\n", op, reg0, reg1);
    }

    if (ra && ra->loc[ins->dest] >= 0) {
        char b2[32];
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movd %s, %s\n",
                           loc_str(b2, ra, ins->dest, x64, syntax), result);
        else
            strbuf_appendf(sb, "    movd %s, %s\n", result,
                           loc_str(b2, ra, ins->dest, x64, syntax));
    } else {
        char b2[32];
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movss %s, %s\n",
                           loc_str(b2, ra, ins->dest, x64, syntax), result);
        else
            strbuf_appendf(sb, "    movss %s, %s\n", result,
                           loc_str(b2, ra, ins->dest, x64, syntax));
    }
    release_xmm_temp(sb, syntax, r1, spill1);
    release_xmm_temp(sb, syntax, r0, spill0);
}

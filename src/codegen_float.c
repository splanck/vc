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
    int r0 = regalloc_xmm_acquire();
    int r1 = regalloc_xmm_acquire();
    const char *reg0 = fmt_reg(regalloc_xmm_name(r0), syntax);
    const char *reg1 = fmt_reg(regalloc_xmm_name(r1), syntax);
    if (syntax == ASM_INTEL) {
        strbuf_appendf(sb, "    movd %s, %s\n", reg0,
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    movd %s, %s\n", reg1,
                       loc_str(b1, ra, ins->src2, x64, syntax));
        strbuf_appendf(sb, "    %s %s, %s\n", op, reg1, reg0);
        if (ra && ra->loc[ins->dest] >= 0) {
            char b2[32];
            strbuf_appendf(sb, "    movd %s, %s\n",
                           loc_str(b2, ra, ins->dest, x64, syntax), reg1);
        } else {
            char b2[32];
            strbuf_appendf(sb, "    movss %s, %s\n",
                           loc_str(b2, ra, ins->dest, x64, syntax), reg1);
        }
    } else {
        strbuf_appendf(sb, "    movd %s, %s\n",
                       loc_str(b1, ra, ins->src1, x64, syntax), reg0);
        strbuf_appendf(sb, "    movd %s, %s\n",
                       loc_str(b1, ra, ins->src2, x64, syntax), reg1);
        strbuf_appendf(sb, "    %s %s, %s\n", op, reg0, reg1);
        if (ra && ra->loc[ins->dest] >= 0) {
            char b2[32];
            strbuf_appendf(sb, "    movd %s, %s\n", reg1,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        } else {
            char b2[32];
            strbuf_appendf(sb, "    movss %s, %s\n", reg1,
                           loc_str(b2, ra, ins->dest, x64, syntax));
        }
    }
    ins->dest = r1;
    regalloc_xmm_release(r1);
    regalloc_xmm_release(r0);
}

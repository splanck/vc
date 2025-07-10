/*
 * Argument handling for function calls.
 *
 * This module implements IR_ARG lowering and tracks the number of
 * stack bytes pushed for the current call.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "codegen_args.h"
#include "regalloc_x86.h"
#include "ast.h"

/* total bytes pushed for the current function call */
size_t arg_stack_bytes = 0;

/* Helper to format a register name. */
static const char *reg_str(int reg, asm_syntax_t syntax)
{
    const char *name = regalloc_reg_name(reg);
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

/* Format the location for operand `id`. */
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

/* Push an argument onto the stack (IR_ARG). */
void emit_arg(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax)
{
    char b1[32];
    const char *sp = (syntax == ASM_INTEL)
                     ? (x64 ? "rsp" : "esp")
                     : (x64 ? "%rsp" : "%esp");
    type_kind_t t = (type_kind_t)ins->imm;
    size_t sz = x64 ? 8 : 4;
    if (t == TYPE_FLOAT)
        sz = 4;
    else if (t == TYPE_DOUBLE)
        sz = 8;
    else if (t == TYPE_LDOUBLE)
        sz = 10;
    if (t == TYPE_FLOAT) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    sub %s, 4\n", sp);
        else
            strbuf_appendf(sb, "    sub $4, %s\n", sp);
        strbuf_appendf(sb, "    movd %s, %%xmm0\n",
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    movss %%xmm0, (%s)\n", sp);
    } else if (t == TYPE_DOUBLE) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    sub %s, 8\n", sp);
        else
            strbuf_appendf(sb, "    sub $8, %s\n", sp);
        strbuf_appendf(sb, "    movq %s, %%xmm0\n",
                       loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    movsd %%xmm0, (%s)\n", sp);
    } else if (t == TYPE_LDOUBLE) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    sub %s, 10\n", sp);
        else
            strbuf_appendf(sb, "    sub $10, %s\n", sp);
        strbuf_appendf(sb, "    fldt %s\n", loc_str(b1, ra, ins->src1, x64, syntax));
        strbuf_appendf(sb, "    fstpt (%s)\n", sp);
    } else {
        const char *sfx = x64 ? "q" : "l";
        strbuf_appendf(sb, "    push%s %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax));
    }
    arg_stack_bytes += sz;
}


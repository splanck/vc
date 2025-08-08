/*
 * Emitters for IR store instructions.
 *
 * These helpers move register values into memory after register
 * allocation. The `x64` flag selects between 32- and 64-bit encodings.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "codegen_mem.h"
#include "codegen_loadstore.h"
#include "regalloc_x86.h"


#define SCRATCH_REG 0

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

/*
 * Store a value to a named location (IR_STORE).
 *
 * Register allocation expectations:
 *   - `src1` contains the value to store and may live in a register or on
 *     the stack according to `ra`.
 *   - `name` designates the memory destination.
 */
void emit_store(strbuf_t *sb, ir_instr_t *ins,
                regalloc_t *ra, int x64,
                asm_syntax_t syntax)
{
    char b1[32];
    const char *sfx = (x64 && ins->type != TYPE_INT) ? "q" : "l";
    char sbuf[32];
    const char *dst = fmt_stack(sbuf, ins->name, x64, syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dst,
                       loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax), dst);
}

/*
 * Store a value via a pointer operand (IR_STORE_PTR).
 *
 * Register allocation expectations:
 *   - `src1` holds the destination address.
 *   - `src2` contains the value to store.
 */
void emit_store_ptr(strbuf_t *sb, ir_instr_t *ins,
                    regalloc_t *ra, int x64,
                    asm_syntax_t syntax)
{
    char b1[32];
    const char *sfx = (x64 && ins->type != TYPE_INT) ? "q" : "l";
    if (syntax == ASM_INTEL) {
        char b2[32];
        const char *addr = loc_str(b2, ra, ins->src1, x64, syntax);
        char buf[32];
        const char *dst = addr;
        if (ra && ins->src1 > 0 && ra->loc[ins->src1] >= 0) {
            snprintf(buf, sizeof(buf), "[%s]", addr);
            dst = buf;
        }
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dst,
                       loc_str(b1, ra, ins->src2, x64, syntax));
    } else {
        char b2[32];
        const char *addr = loc_str(b2, ra, ins->src1, x64, syntax);
        char buf[32];
        const char *dst = addr;
        if (ra && ins->src1 > 0 && ra->loc[ins->src1] >= 0) {
            snprintf(buf, sizeof(buf), "(%s)", addr);
            dst = buf;
        }
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax), dst);
    }
}

/*
 * Store a value to an indexed location (IR_STORE_IDX).
 *
 * Register allocation expectations:
 *   - `src1` provides the index.
 *   - `src2` is the value to store.
 */
void emit_store_idx(strbuf_t *sb, ir_instr_t *ins,
                    regalloc_t *ra, int x64,
                    asm_syntax_t syntax)
{
    char b1[32];
    const char *sfx = (x64 && ins->type != TYPE_INT) ? "q" : "l";
    char basebuf[32];
    const char *base = fmt_stack(basebuf, ins->name, x64, syntax);
    int scale = idx_scale(ins, x64);
    if (scale != 1 && scale != 2 && scale != 4 && scale != 8)
        scale = 1;
    if (syntax == ASM_INTEL) {
        char b2[32];
        strbuf_appendf(sb, "    mov%s %s(,%s,%d), %s\n", sfx, base,
                       loc_str(b2, ra, ins->src1, x64, syntax), scale,
                       loc_str(b1, ra, ins->src2, x64, syntax));
    } else {
        char b2[32];
        strbuf_appendf(sb, "    mov%s %s, %s(,%s,%d)\n", sfx,
                       loc_str(b1, ra, ins->src2, x64, syntax),
                       base,
                       loc_str(b2, ra, ins->src1, x64, syntax), scale);
    }
}


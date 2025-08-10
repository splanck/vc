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
    const char *src;

    if (ra && ins->src1 > 0 && ra->loc[ins->src1] < 0) {
        /* `src1` spilled: move through scratch register to avoid mem-to-mem. */
        const char *scratch = reg_str(SCRATCH_REG, syntax);
        const char *slot = loc_str(b1, ra, ins->src1, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, scratch);
        src = scratch;
    } else {
        src = loc_str(b1, ra, ins->src1, x64, syntax);
    }

    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dst, src);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, src, dst);
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
    char addrbuf[32];
    char dstbuf[32];
    const char *dst;

    if (ra && ins->src1 > 0 && ra->loc[ins->src1] < 0) {
        /* `src1` spilled: load address into scratch first. */
        const char *scratch = reg_str(SCRATCH_REG, syntax);
        const char *slot = loc_str(addrbuf, ra, ins->src1, x64, syntax);
        const char *psfx = x64 ? "q" : "l";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, slot, scratch);
        if (syntax == ASM_INTEL)
            snprintf(dstbuf, sizeof(dstbuf), "[%s]", scratch);
        else
            snprintf(dstbuf, sizeof(dstbuf), "(%s)", scratch);
        dst = dstbuf;
    } else {
        const char *addr = loc_str(addrbuf, ra, ins->src1, x64, syntax);
        dst = addr;
        if (ra && ins->src1 > 0 && ra->loc[ins->src1] >= 0) {
            if (syntax == ASM_INTEL)
                snprintf(dstbuf, sizeof(dstbuf), "[%s]", addr);
            else
                snprintf(dstbuf, sizeof(dstbuf), "(%s)", addr);
            dst = dstbuf;
        }
    }

    const char *src;
    if (ra && ins->src2 > 0 && ra->loc[ins->src2] < 0) {
        /* Load spilled value into scratch register. */
        const char *scratch = reg_str(SCRATCH_REG, syntax);
        const char *slot = loc_str(b1, ra, ins->src2, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, scratch);
        src = scratch;
    } else {
        src = loc_str(b1, ra, ins->src2, x64, syntax);
    }

    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dst, src);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, src, dst);
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
    int manual = (scale != 1 && scale != 2 && scale != 4 && scale != 8);

    const char *val;
    if (ra && ins->src2 > 0 && ra->loc[ins->src2] < 0) {
        /* `src2` spilled: move value through scratch register. */
        const char *scratch = reg_str(SCRATCH_REG, syntax);
        const char *slot = loc_str(b1, ra, ins->src2, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, scratch);
        val = scratch;
    } else {
        val = loc_str(b1, ra, ins->src2, x64, syntax);
    }

    char b2[32];
    const char *idx;
    const char *psfx = x64 ? "q" : "l";
    if (manual) {
        /* Multiply index into scratch register for arbitrary scales. */
        const char *scratch = reg_str(SCRATCH_REG, syntax);
        const char *src = loc_str(b2, ra, ins->src1, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, scratch, src);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, src, scratch);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    imul%s %s, %s, %d\n", psfx, scratch, scratch, scale);
        else
            strbuf_appendf(sb, "    imul%s $%d, %s, %s\n", psfx, scale, scratch, scratch);
        idx = scratch;
        scale = 1;
    } else if (ra && ins->src1 > 0 && ra->loc[ins->src1] < 0) {
        /* Load spilled index into scratch register. */
        const char *scratch = reg_str(SCRATCH_REG, syntax);
        const char *src = loc_str(b2, ra, ins->src1, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, scratch, src);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, src, scratch);
        idx = scratch;
    } else {
        idx = loc_str(b2, ra, ins->src1, x64, syntax);
    }

    if (syntax == ASM_INTEL) {
        const char *b = base;
        char inner[32];
        size_t len = strlen(base);
        if (len >= 2 && base[0] == '[' && base[len - 1] == ']') {
            /* Remove surrounding brackets produced by fmt_stack. */
            snprintf(inner, sizeof(inner), "%.*s", (int)(len - 2), base + 1);
            b = inner;
        }
        if (scale == 1)
            strbuf_appendf(sb, "    mov%s [%s+%s], %s\n", sfx, b, idx, val);
        else
            strbuf_appendf(sb, "    mov%s [%s+%s*%d], %s\n", sfx, b, idx, scale, val);
    } else {
        strbuf_appendf(sb, "    mov%s %s, %s(,%s,%d)\n", sfx, val, base, idx, scale);
    }
}


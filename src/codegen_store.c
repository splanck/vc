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
#include "regalloc.h"



/* Forward declaration for register name helper. */
static const char *reg_str(int reg, int size, asm_syntax_t syntax);

/* Determine operand size from the IR type. */
static int op_size(type_kind_t t, int x64)
{
    switch (t) {
    case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL:
        return 1;
    case TYPE_SHORT: case TYPE_USHORT:
        return 2;
    case TYPE_DOUBLE: case TYPE_LLONG: case TYPE_ULLONG:
    case TYPE_FLOAT_COMPLEX:
        return 8;
    case TYPE_LDOUBLE:
        return 10;
    case TYPE_DOUBLE_COMPLEX:
        return 16;
    case TYPE_LDOUBLE_COMPLEX:
        return 20;
    case TYPE_PTR:
        return x64 ? 8 : 4;
    default:
        return 4;
    }
}

/* Return subregister name for register `reg` and size. */
static const char *reg_subreg(int reg, int size, asm_syntax_t syntax)
{
    if (reg < 0 || reg >= REGALLOC_NUM_REGS)
        reg = 0;
    static const char *regs8_att[REGALLOC_NUM_REGS] =
        {"%al", "%bl", "%cl", "%dl", "%sil", "%dil"};
    static const char *regs8_intel[REGALLOC_NUM_REGS] =
        {"al", "bl", "cl", "dl", "sil", "dil"};
    static const char *regs16_att[REGALLOC_NUM_REGS] =
        {"%ax", "%bx", "%cx", "%dx", "%si", "%di"};
    static const char *regs16_intel[REGALLOC_NUM_REGS] =
        {"ax", "bx", "cx", "dx", "si", "di"};
    if (size == 1)
        return (syntax == ASM_INTEL) ? regs8_intel[reg] : regs8_att[reg];
    if (size == 2)
        return (syntax == ASM_INTEL) ? regs16_intel[reg] : regs16_att[reg];
    return reg_str(reg, size, syntax);
}

/* Helper to format a register name. */
static const char *reg_str(int reg, int size, asm_syntax_t syntax)
{
    const char *name = (size == 4) ? regalloc_reg_name32(reg)
                                  : regalloc_reg_name(reg);
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

/* Format the location for operand `id`. */
static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64,
                           int size, asm_syntax_t syntax)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return reg_str(loc, size, syntax);
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

/* Format memory operand `base` with additional byte offset. */
static void fmt_mem_off(char buf[64], const char *base, int off,
                        asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL) {
        size_t len = strlen(base);
        if (len >= 2 && base[0] == '[' && base[len - 1] == ']')
            snprintf(buf, 64, "[%.*s+%d]", (int)(len - 2), base + 1, off);
        else
            snprintf(buf, 64, "%s+%d", base, off);
    } else {
        const char *paren = strchr(base, '(');
        if (paren) {
            char num[32];
            snprintf(num, sizeof(num), "%.*s", (int)(paren - base), base);
            long val = strtol(num, NULL, 10);
            snprintf(buf, 64, "%ld%s", val + off, paren);
        } else {
            snprintf(buf, 64, "%s+%d", base, off);
        }
    }
}

static int is_memop(const char *s)
{
    return s && (strchr(s, '[') || strchr(s, '('));
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
    int size = op_size(ins->type, x64);
    char sbuf[32];
    const char *dst = fmt_stack(sbuf, ins->name, x64, syntax);

    if (size == 10) {
        const char *src = loc_str(b1, ra, ins->src1, x64, size, syntax);
        if (is_memop(src) && is_memop(dst)) {
            if (syntax == ASM_INTEL) {
                strbuf_appendf(sb, "    fld tword ptr %s\n", src);
                strbuf_appendf(sb, "    fstp tword ptr %s\n", dst);
            } else {
                strbuf_appendf(sb, "    fldt %s\n", src);
                strbuf_appendf(sb, "    fstpt %s\n", dst);
            }
            return;
        }
    }

    const char *src0 = loc_str(b1, ra, ins->src1, x64, size, syntax);
    if ((size == 16 || size == 20) && is_memop(dst) && is_memop(src0)) {
        const char *src = src0;
        int xr = regalloc_xmm_acquire();
        if (xr < 0) {
            fprintf(stderr, "emit_store: XMM register allocation failed\n");
            strbuf_appendf(sb, "    # XMM register allocation failed\n");
            return;
        }
        const char *xreg = regalloc_xmm_name(xr);
        if (syntax == ASM_INTEL) {
            strbuf_appendf(sb, "    movdqu %s, %s\n", xreg, src);
            strbuf_appendf(sb, "    movdqu %s, %s\n", dst, xreg);
        } else {
            strbuf_appendf(sb, "    movdqu %s, %s\n", src, xreg);
            strbuf_appendf(sb, "    movdqu %s, %s\n", xreg, dst);
        }
        regalloc_xmm_release(xr);
        if (size == 20) {
            char soff[64];
            char doff[64];
            fmt_mem_off(soff, src, 16, syntax);
            fmt_mem_off(doff, dst, 16, syntax);
            const char *scratch = reg_str(REGALLOC_SCRATCH_REG, 4, syntax);
            if (syntax == ASM_INTEL) {
                strbuf_appendf(sb, "    movl %s, %s\n", scratch, soff);
                strbuf_appendf(sb, "    movl %s, %s\n", doff, scratch);
            } else {
                strbuf_appendf(sb, "    movl %s, %s\n", soff, scratch);
                strbuf_appendf(sb, "    movl %s, %s\n", scratch, doff);
            }
        }
        return;
    }

    const char *sfx;
    if (size == 1)
        sfx = "b";
    else if (size == 2)
        sfx = "w";
    else if (size == 8 && x64)
        sfx = "q";
    else
        sfx = "l";
    const char *src;

    if (ra && ins->src1 > 0 && ra->loc[ins->src1] < 0) {
        /* `src1` spilled: move through scratch register to avoid mem-to-mem. */
        const char *scratch = (size <= 2)
                                  ? reg_subreg(REGALLOC_SCRATCH_REG, size, syntax)
                                  : reg_str(REGALLOC_SCRATCH_REG, size, syntax);
        const char *slot = loc_str(b1, ra, ins->src1, x64, size, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, scratch);
        src = scratch;
    } else {
        if (ra && ins->src1 > 0 && ra->loc[ins->src1] >= 0 && size <= 2)
            src = reg_subreg(ra->loc[ins->src1], size, syntax);
        else
            src = loc_str(b1, ra, ins->src1, x64, size, syntax);
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
    int size = op_size(ins->type, x64);
    const char *sfx;
    if (size == 1)
        sfx = "b";
    else if (size == 2)
        sfx = "w";
    else if (size == 8 && x64)
        sfx = "q";
    else
        sfx = "l";
    char addrbuf[32];
    char dstbuf[32];
    const char *dst;

    int addr_spill = ra && ins->src1 > 0 && ra->loc[ins->src1] < 0;
    int val_spill = ra && ins->src2 > 0 && ra->loc[ins->src2] < 0;

    if (addr_spill) {
        /* `src1` spilled: load address into scratch first. */
        const char *scratch = reg_str(REGALLOC_SCRATCH_REG, x64 ? 8 : 4, syntax);
        const char *slot = loc_str(addrbuf, ra, ins->src1, x64, x64 ? 8 : 4, syntax);
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
        const char *addr = loc_str(addrbuf, ra, ins->src1, x64, x64 ? 8 : 4, syntax);
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
    if (val_spill) {
        /* Load spilled value into scratch register. */
        int scratch_idx = addr_spill ? REGALLOC_SCRATCH_REG2 : REGALLOC_SCRATCH_REG;
        const char *scratch = (size <= 2)
                                  ? reg_subreg(scratch_idx, size, syntax)
                                  : reg_str(scratch_idx, size, syntax);
        const char *slot = loc_str(b1, ra, ins->src2, x64, size, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, scratch);
        src = scratch;
    } else {
        if (ra && ins->src2 > 0 && ra->loc[ins->src2] >= 0 && size <= 2)
            src = reg_subreg(ra->loc[ins->src2], size, syntax);
        else
            src = loc_str(b1, ra, ins->src2, x64, size, syntax);
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
    int size = op_size(ins->type, x64);
    const char *sfx;
    if (size == 1)
        sfx = "b";
    else if (size == 2)
        sfx = "w";
    else if (size == 8 && x64)
        sfx = "q";
    else
        sfx = "l";
    char basebuf[32];
    const char *base = fmt_stack(basebuf, ins->name, x64, syntax);
    int scale = idx_scale(ins, x64);
    int manual = (scale != 1 && scale != 2 && scale != 4 && scale != 8);
    int idx_spill = ra && ins->src1 > 0 && ra->loc[ins->src1] < 0;
    int val_spill = ra && ins->src2 > 0 && ra->loc[ins->src2] < 0;
    int idx_needs_scratch = manual || idx_spill;

    const char *val;
    if (val_spill) {
        /* `src2` spilled: move value through a scratch register. */
        int scratch_idx = idx_needs_scratch ? REGALLOC_SCRATCH_REG2 : REGALLOC_SCRATCH_REG;
        const char *scratch = (size <= 2)
                                  ? reg_subreg(scratch_idx, size, syntax)
                                  : reg_str(scratch_idx, size, syntax);
        const char *slot = loc_str(b1, ra, ins->src2, x64, size, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, slot);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, scratch);
        val = scratch;
    } else {
        if (ra && ins->src2 > 0 && ra->loc[ins->src2] >= 0 && size <= 2)
            val = reg_subreg(ra->loc[ins->src2], size, syntax);
        else
            val = loc_str(b1, ra, ins->src2, x64, size, syntax);
    }

    char b2[32];
    const char *idx;
    const char *psfx = x64 ? "q" : "l";
    if (manual) {
        /* Multiply index into scratch register for arbitrary scales. */
        const char *scratch = reg_str(REGALLOC_SCRATCH_REG, x64 ? 8 : 4, syntax);
        const char *src = loc_str(b2, ra, ins->src1, x64, x64 ? 8 : 4, syntax);
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
    } else if (idx_spill) {
        /* Load spilled index into scratch register. */
        const char *scratch = reg_str(REGALLOC_SCRATCH_REG, x64 ? 8 : 4, syntax);
        const char *src = loc_str(b2, ra, ins->src1, x64, x64 ? 8 : 4, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, scratch, src);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, src, scratch);
        idx = scratch;
    } else {
        idx = loc_str(b2, ra, ins->src1, x64, x64 ? 8 : 4, syntax);
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


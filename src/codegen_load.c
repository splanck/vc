/*
 * Emitters for IR load instructions.
 *
 * These helpers move values from memory into registers after register
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

static int is_signed(type_kind_t t)
{
    return t == TYPE_CHAR || t == TYPE_SHORT;
}
/* Helper to format a register name. */
static const char *reg_str(int reg, asm_syntax_t syntax)
{
    const char *name = regalloc_reg_name(reg);
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
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
    return reg_str(reg, syntax);
}

static const char *scratch_subreg(int size, asm_syntax_t syntax)
{
    return reg_subreg(REGALLOC_SCRATCH_REG, size, syntax);
}

static void emit_ins(strbuf_t *sb, const char *insn,
                     const char *src, const char *dest,
                     asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    %s %s, %s\n", insn, dest, src);
    else
        strbuf_appendf(sb, "    %s %s, %s\n", insn, src, dest);
}

static void emit_move_with_spill(strbuf_t *sb, const char *sfx,
                                 const char *src, const char *dest,
                                 const char *slot, int spill,
                                 asm_syntax_t syntax);

static void emit_typed_load(strbuf_t *sb, type_kind_t type, int x64,
                            const char *src, const char *dest,
                            const char *slot, int spill,
                            asm_syntax_t syntax)
{
    int size = op_size(type, x64);
    if (size == 1 || size == 2) {
        const char *inst;
        const char *spill_inst = (size == 1) ? "movb" : "movw";
        if (size == 1)
            inst = is_signed(type)
                       ? (x64 ? "movsbq" : "movsbl")
                       : (x64 ? "movzbq" : "movzbl");
        else
            inst = is_signed(type)
                       ? (x64 ? "movswq" : "movswl")
                       : (x64 ? "movzwq" : "movzwl");
        emit_ins(sb, inst, src, dest, syntax);
        if (spill) {
            const char *low = scratch_subreg(size, syntax);
            emit_ins(sb, spill_inst, low, slot, syntax);
        }
    } else {
        const char *sfx = (x64 && type != TYPE_INT) ? "q" : "l";
        emit_move_with_spill(sb, sfx, src, dest, slot, spill, syntax);
    }
}

/* Move from `src` to `dest` and optionally spill to `slot`. */
static void emit_move_with_spill(strbuf_t *sb, const char *sfx,
                                 const char *src, const char *dest,
                                 const char *slot, int spill,
                                 asm_syntax_t syntax)
{
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, src);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, src, dest);
    if (spill) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, dest);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
    }
}

/* Helper to format a register name. */
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
 * Load a value from memory into the destination location (IR_LOAD).
 *
 * Register allocation expectations:
 *   - `dest` may reside in a register or a stack slot as determined by `ra`.
 *     When spilled, REGALLOC_SCRATCH_REG is used and the result is written back.
 *   - `name` is the memory operand to load from and does not require a
 *     register.
 */
void emit_load(strbuf_t *sb, ir_instr_t *ins,
               regalloc_t *ra, int x64,
               asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(REGALLOC_SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char sbuf[32];
    const char *src = fmt_stack(sbuf, ins->name, x64, syntax);
    emit_typed_load(sb, ins->type, x64, src, dest, slot, spill, syntax);
}

/*
 * Load a value via a pointer operand (IR_LOAD_PTR).
 *
 * Register allocation expectations:
 *   - `src1` holds the address to load from; the allocator may place it in
 *     a register or stack slot.
 *   - `dest` follows the same rules as for emit_load.
 */
void emit_load_ptr(strbuf_t *sb, ir_instr_t *ins,
                   regalloc_t *ra, int x64,
                   asm_syntax_t syntax)
{
    char b1[32];
    char destb[32];
    char mem[32];
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(REGALLOC_SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    const char *addr = loc_str(b1, ra, ins->src1, x64, syntax);
    char srcbuf[32];
    const char *src;
    if (ra && ins->src1 > 0 && ra->loc[ins->src1] < 0) {
        /* `src1` spilled: load address into scratch first. */
        const char *scratch = reg_str(REGALLOC_SCRATCH_REG, syntax);
        const char *psfx = x64 ? "q" : "l";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, scratch, addr);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, addr, scratch);
        if (syntax == ASM_INTEL)
            snprintf(srcbuf, sizeof(srcbuf), "[%s]", scratch);
        else
            snprintf(srcbuf, sizeof(srcbuf), "(%s)", scratch);
        src = srcbuf;
    } else if (ra && ins->src1 > 0 && ra->loc[ins->src1] >= 0) {
        if (syntax == ASM_INTEL)
            snprintf(srcbuf, sizeof(srcbuf), "[%s]", addr);
        else
            snprintf(srcbuf, sizeof(srcbuf), "(%s)", addr);
        src = srcbuf;
    } else {
        src = addr;
    }
    emit_typed_load(sb, ins->type, x64, src, dest, slot, spill, syntax);
}

/*
 * Load from an indexed location (IR_LOAD_IDX).
 *
 * Register allocation expectations:
 *   - `src1` provides the index value.
 *   - `dest` is handled as in emit_load.
 */
void emit_load_idx(strbuf_t *sb, ir_instr_t *ins,
                   regalloc_t *ra, int x64,
                   asm_syntax_t syntax)
{
    char b1[32];
    char destb[32];
    char mem[32];
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(REGALLOC_SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char srcbuf[64];
    char basebuf[32];
    const char *base = fmt_stack(basebuf, ins->name, x64, syntax);
    int scale = idx_scale(ins, x64);
    int manual = (scale != 1 && scale != 2 && scale != 4 && scale != 8);
    int idx_spill = ra && ins->src1 > 0 && ra->loc[ins->src1] < 0;

    const char *idx;
    const char *psfx = x64 ? "q" : "l";
    if (manual) {
        /* Multiply index into scratch register for arbitrary scales. */
        const char *scratch = reg_str(REGALLOC_SCRATCH_REG, syntax);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
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
        const char *scratch = reg_str(REGALLOC_SCRATCH_REG, syntax);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, scratch, src);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", psfx, src, scratch);
        idx = scratch;
    } else {
        idx = loc_str(b1, ra, ins->src1, x64, syntax);
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
            snprintf(srcbuf, sizeof(srcbuf), "[%s+%s]", b, idx);
        else
            snprintf(srcbuf, sizeof(srcbuf), "[%s+%s*%d]", b, idx, scale);
    } else {
        snprintf(srcbuf, sizeof(srcbuf), "%s(,%s,%d)", base, idx, scale);
    }
    emit_typed_load(sb, ins->type, x64, srcbuf, dest, slot, spill, syntax);
}


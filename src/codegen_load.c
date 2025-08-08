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

#define SCRATCH_REG 0

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
 * Load a value from memory into the destination location (IR_LOAD).
 *
 * Register allocation expectations:
 *   - `dest` may reside in a register or a stack slot as determined by `ra`.
 *     When spilled, SCRATCH_REG is used and the result is written back.
 *   - `name` is the memory operand to load from and does not require a
 *     register.
 */
void emit_load(strbuf_t *sb, ir_instr_t *ins,
               regalloc_t *ra, int x64,
               asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    const char *sfx = (x64 && ins->type != TYPE_INT) ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char sbuf[32];
    const char *src = fmt_stack(sbuf, ins->name, x64, syntax);
    emit_move_with_spill(sb, sfx, src, dest, slot, spill, syntax);
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
    const char *sfx = (x64 && ins->type != TYPE_INT) ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    const char *addr = loc_str(b1, ra, ins->src1, x64, syntax);
    char srcbuf[32];
    const char *src;
    if (ra && ins->src1 > 0 && ra->loc[ins->src1] >= 0) {
        if (syntax == ASM_INTEL)
            snprintf(srcbuf, sizeof(srcbuf), "[%s]", addr);
        else
            snprintf(srcbuf, sizeof(srcbuf), "(%s)", addr);
        src = srcbuf;
    } else {
        src = addr;
    }
    emit_move_with_spill(sb, sfx, src, dest, slot, spill, syntax);
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
    const char *sfx = (x64 && ins->type != TYPE_INT) ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char srcbuf[64];
    char basebuf[32];
    const char *base = fmt_stack(basebuf, ins->name, x64, syntax);
    int scale = idx_scale(ins, x64);
    snprintf(srcbuf, sizeof(srcbuf), "%s(,%s,%d)",
             base, loc_str(b1, ra, ins->src1, x64, syntax), scale);
    emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
}


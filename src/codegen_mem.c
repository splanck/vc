/*
 * Emitters for memory-related IR instructions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "codegen_mem.h"
#include "regalloc_x86.h"

#define SCRATCH_REG 0

static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return regalloc_reg_name(loc);
    if (x64)
        snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
    else
        snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    return buf;
}

void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64)
{
    char buf1[32];
    char buf2[32];
    char destbuf[32];
    char membuf[32];
    const char *sfx = x64 ? "q" : "l";
    const char *bp = x64 ? "%rbp" : "%ebp";
    int dest_spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest_reg = dest_spill ? regalloc_reg_name(SCRATCH_REG)
                                      : loc_str(destbuf, ra, ins->dest, x64);
    const char *dest_mem = loc_str(membuf, ra, ins->dest, x64);

    switch (ins->op) {
    case IR_CONST:
        strbuf_appendf(sb, "    mov%s $%lld, %s\n", sfx, ins->imm, dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_LOAD:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ins->name, dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_STORE:
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64),
                       ins->name);
        break;
    case IR_LOAD_PARAM: {
        int off = 8 + ins->imm * (x64 ? 8 : 4);
        strbuf_appendf(sb, "    mov%s %d(%s), %s\n", sfx, off, bp, dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    }
    case IR_STORE_PARAM: {
        int off = 8 + ins->imm * (x64 ? 8 : 4);
        strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), off, bp);
        break;
    }
    case IR_ADDR:
        strbuf_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name, dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_LOAD_PTR:
        strbuf_appendf(sb, "    mov%s (%s), %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64), dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_STORE_PTR:
        strbuf_appendf(sb, "    mov%s %s, (%s)\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       loc_str(buf2, ra, ins->src1, x64));
        break;
    case IR_LOAD_IDX:
        strbuf_appendf(sb, "    mov%s %s(,%s,4), %s\n", sfx,
                       ins->name,
                       loc_str(buf1, ra, ins->src1, x64), dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_STORE_IDX:
        strbuf_appendf(sb, "    mov%s %s, %s(,%s,4)\n", sfx,
                       loc_str(buf1, ra, ins->src2, x64),
                       ins->name,
                       loc_str(buf2, ra, ins->src1, x64));
        break;
    case IR_ARG:
        strbuf_appendf(sb, "    push%s %s\n", sfx,
                       loc_str(buf1, ra, ins->src1, x64));
        break;
    case IR_GLOB_STRING:
        strbuf_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name, dest_reg);
        if (dest_spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest_reg, dest_mem);
        break;
    case IR_GLOB_VAR:
        break;
    case IR_GLOB_ARRAY:
        break;
    case IR_GLOB_UNION:
        break;
    case IR_GLOB_STRUCT:
        break;
    default:
        break;
    }
}


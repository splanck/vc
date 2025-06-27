/*
 * Emitters for memory-related IR instructions.
 *
 * Operations such as loads, stores and address calculations are lowered
 * here after registers have been assigned.  Spilled values are written to
 * or read from the stack as dictated by the register allocator.  The
 * `x64` flag chooses between 32- and 64-bit addressing modes and register
 * names.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include "codegen_mem.h"
#include "regalloc_x86.h"

#define SCRATCH_REG 0

/* Format the register or stack location for operand `id`. */
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

/* Load an immediate constant (IR_CONST).
 *
 *  dest - value receiving the constant
 *  imm  - value to load
 */
static void emit_const(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s $%lld, %s\n", sfx, ins->imm, dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Load a variable by name (IR_LOAD).
 *
 *  dest - result register
 *  name - symbol to read from
 */
static void emit_load(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ins->name, dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Store a value to a variable (IR_STORE).
 *
 *  src1 - value to store
 *  name - destination symbol
 */
static void emit_store(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64)
{
    char b1[32];
    const char *sfx = x64 ? "q" : "l";
    strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64), ins->name);
}

/* Load a function parameter (IR_LOAD_PARAM).
 *
 *  dest  - receives parameter value
 *  imm   - parameter index
 */
static void emit_load_param(strbuf_t *sb, ir_instr_t *ins,
                            regalloc_t *ra, int x64)
{
    char destb[32];
    char mem[32];
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    int off = 8 + ins->imm * (x64 ? 8 : 4);
    strbuf_appendf(sb, "    mov%s %d(%s), %s\n", sfx, off, bp, dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Store a value to a parameter slot (IR_STORE_PARAM).
 *
 *  src1 - value to store
 *  imm  - parameter index
 */
static void emit_store_param(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64)
{
    char b1[32];
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sfx = x64 ? "q" : "l";
    int off = 8 + ins->imm * (x64 ? 8 : 4);
    strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx,
                   loc_str(b1, ra, ins->src1, x64), off, bp);
}

/* Take the address of a symbol (IR_ADDR).
 *
 *  dest - result register
 *  name - symbol whose address is taken
 */
static void emit_addr(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name, dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Load from a pointer (IR_LOAD_PTR).
 *
 *  src1 - address to read from
 *  dest - result location
 */
static void emit_load_ptr(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64)
{
    char b1[32];
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s (%s), %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64), dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Store through a pointer (IR_STORE_PTR).
 *
 *  src1 - pointer
 *  src2 - value to store
 */
static void emit_store_ptr(strbuf_t *sb, ir_instr_t *ins,
                           regalloc_t *ra, int x64)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    strbuf_appendf(sb, "    mov%s %s, (%s)\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   loc_str(b2, ra, ins->src1, x64));
}

/* Load from an indexed symbol (IR_LOAD_IDX).
 *
 *  src1 - index value
 *  name - base symbol
 *  dest - result
 */
static void emit_load_idx(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64)
{
    char b1[32];
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s %s(,%s,4), %s\n", sfx,
                   ins->name, loc_str(b1, ra, ins->src1, x64), dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Store to an indexed symbol (IR_STORE_IDX).
 *
 *  src1 - index
 *  src2 - value
 *  name - base symbol
 */
static void emit_store_idx(strbuf_t *sb, ir_instr_t *ins,
                           regalloc_t *ra, int x64)
{
    char b1[32];
    char b2[32];
    const char *sfx = x64 ? "q" : "l";
    strbuf_appendf(sb, "    mov%s %s, %s(,%s,4)\n", sfx,
                   loc_str(b1, ra, ins->src2, x64),
                   ins->name,
                   loc_str(b2, ra, ins->src1, x64));
}

/* Push an argument (IR_ARG). */
static void emit_arg(strbuf_t *sb, ir_instr_t *ins,
                     regalloc_t *ra, int x64)
{
    char b1[32];
    const char *sfx = x64 ? "q" : "l";
    strbuf_appendf(sb, "    push%s %s\n", sfx,
                   loc_str(b1, ra, ins->src1, x64));
}

/* Load address of a string literal (IR_GLOB_STRING). */
static void emit_glob_string(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? regalloc_reg_name(SCRATCH_REG)
                             : loc_str(destb, ra, ins->dest, x64);
    const char *slot = loc_str(mem, ra, ins->dest, x64);
    strbuf_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name, dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/*
 * Emit x86 for load/store and other memory instructions.
 *
 * Operand locations are taken from the register allocator.  If the
 * destination is spilled, a temporary register is used and the value is
 * stored back to the stack.  The `x64` flag determines pointer size and
 * register names so that either 32- or 64-bit code can be produced.
 */
void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64)
{
    switch (ins->op) {
    case IR_CONST:
        emit_const(sb, ins, ra, x64);
        break;
    case IR_LOAD:
        emit_load(sb, ins, ra, x64);
        break;
    case IR_STORE:
        emit_store(sb, ins, ra, x64);
        break;
    case IR_LOAD_PARAM:
        emit_load_param(sb, ins, ra, x64);
        break;
    case IR_STORE_PARAM:
        emit_store_param(sb, ins, ra, x64);
        break;
    case IR_ADDR:
        emit_addr(sb, ins, ra, x64);
        break;
    case IR_LOAD_PTR:
        emit_load_ptr(sb, ins, ra, x64);
        break;
    case IR_STORE_PTR:
        emit_store_ptr(sb, ins, ra, x64);
        break;
    case IR_LOAD_IDX:
        emit_load_idx(sb, ins, ra, x64);
        break;
    case IR_STORE_IDX:
        emit_store_idx(sb, ins, ra, x64);
        break;
    case IR_ARG:
        emit_arg(sb, ins, ra, x64);
        break;
    case IR_GLOB_STRING:
        emit_glob_string(sb, ins, ra, x64);
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


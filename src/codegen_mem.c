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
#include "codegen_loadstore.h"
#include "codegen_args.h"
#include "regalloc_x86.h"
#include "ast.h"

#define SCRATCH_REG 0

/*
 * Emit a move from `src` to `dest` and optionally spill the result.
 *
 * `sfx` selects between 32- and 64-bit instruction forms.  When `spill`
 * is non-zero, the value in `dest` is written back to `slot` after the
 * move.  The helper is used to implement loads that need to store the
 * result to a stack slot when the destination register was spilled.
 */
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

/* Format the register or stack location for operand `id`. */
/* Format a register name for Intel or AT&T syntax. */
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

/* Load an immediate constant (IR_CONST).
 *
 *  dest - value receiving the constant
 *  imm  - value to load
 */
static void emit_const(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char srcbuf[32];
    if (syntax == ASM_INTEL)
        snprintf(srcbuf, sizeof(srcbuf), "%lld", ins->imm);
    else
        snprintf(srcbuf, sizeof(srcbuf), "$%lld", ins->imm);
    emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
}

/* Load a variable by name (IR_LOAD).
 *
 *  dest - result register
 *  name - symbol to read from
 */


/* Store a value to a variable (IR_STORE).
 *
 *  src1 - value to store
 *  name - destination symbol
 */


/* Load a function parameter (IR_LOAD_PARAM).
 *
 *  dest  - receives parameter value
 *  imm   - parameter index
 */
static void emit_load_param(strbuf_t *sb, ir_instr_t *ins,
                            regalloc_t *ra, int x64,
                            asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    const char *bp = (syntax == ASM_INTEL)
                     ? (x64 ? "rbp" : "ebp")
                     : (x64 ? "%rbp" : "%ebp");
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    int off = 8 + ins->imm * (x64 ? 8 : 4);
    char srcbuf[32];
    if (syntax == ASM_INTEL)
        snprintf(srcbuf, sizeof(srcbuf), "[%s+%d]", bp, off);
    else
        snprintf(srcbuf, sizeof(srcbuf), "%d(%s)", off, bp);
    emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
}

/* Store a value to a parameter slot (IR_STORE_PARAM).
 *
 *  src1 - value to store
 *  imm  - parameter index
 */
static void emit_store_param(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64,
                             asm_syntax_t syntax)
{
    char b1[32];
    const char *bp = (syntax == ASM_INTEL)
                     ? (x64 ? "rbp" : "ebp")
                     : (x64 ? "%rbp" : "%ebp");
    const char *sfx = x64 ? "q" : "l";
    int off = 8 + ins->imm * (x64 ? 8 : 4);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %d(%s), %s\n", sfx,
                       off, bp, loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax), off, bp);
}

/* Take the address of a symbol (IR_ADDR).
 *
 *  dest - result register
 *  name - symbol whose address is taken
 */
static void emit_addr(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64,
                      asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char srcbuf[32];
    if (syntax == ASM_INTEL)
        snprintf(srcbuf, sizeof(srcbuf), "%s", ins->name);
    else
        snprintf(srcbuf, sizeof(srcbuf), "$%s", ins->name);
    emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
}

/* Load from a pointer (IR_LOAD_PTR).
 *
 *  src1 - address to read from
 *  dest - result location
 */


/* Load from an indexed symbol (IR_LOAD_IDX).
 *
 *  src1 - index value
 *  name - base symbol
 *  dest - result
 */


/* Load a bit-field value (IR_BFLOAD). */
static void emit_bfload(strbuf_t *sb, ir_instr_t *ins,
                        regalloc_t *ra, int x64,
                        asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    unsigned shift = (unsigned)(ins->imm >> 32);
    unsigned width = (unsigned)(ins->imm & 0xffffffffu);
    unsigned long long mask = (width == 64) ? 0xffffffffffffffffULL
                                            : ((1ULL << width) - 1ULL);
    emit_move_with_spill(sb, sfx, ins->name, dest, slot, 0, syntax);
    if (shift) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    shr%s %s, %u\n", sfx, dest, shift);
        else
            strbuf_appendf(sb, "    shr%s $%u, %s\n", sfx, shift, dest);
    }
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    and%s %s, %llu\n", sfx, dest, mask);
    else
        strbuf_appendf(sb, "    and%s $%llu, %s\n", sfx, mask, dest);
    if (spill)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dest, slot);
}

/* Store a value into a bit-field (IR_BFSTORE). */
static void emit_bfstore(strbuf_t *sb, ir_instr_t *ins,
                         regalloc_t *ra, int x64,
                         asm_syntax_t syntax)
{
    char bval[32];
    const char *sfx = x64 ? "q" : "l";
    unsigned shift = (unsigned)(ins->imm >> 32);
    unsigned width = (unsigned)(ins->imm & 0xffffffffu);
    unsigned long long mask = (width == 64) ? 0xffffffffffffffffULL
                                            : ((1ULL << width) - 1ULL);
    unsigned long long clear = ~((unsigned long long)mask << shift);
    const char *scratch = regalloc_reg_name(SCRATCH_REG);

    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, ins->name);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ins->name, scratch);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    and%s %s, %llu\n", sfx, scratch, clear);
    else
        strbuf_appendf(sb, "    and%s $%llu, %s\n", sfx, clear, scratch);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax),
                       loc_str(bval, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx,
                       loc_str(bval, ra, ins->src1, x64, syntax),
                       x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax));
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    and%s %s, %llu\n", sfx,
                       x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax),
                       mask);
    else
        strbuf_appendf(sb, "    and%s $%llu, %s\n", sfx, mask,
                       x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax));
    if (shift) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    shl%s %s, %u\n", sfx,
                           x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax),
                           shift);
        else
            strbuf_appendf(sb, "    shl%s $%u, %s\n", sfx, shift,
                           x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax));
    }
    strbuf_appendf(sb, "    or%s %s, %s\n", sfx,
                   x64 ? fmt_reg("%rcx", syntax) : fmt_reg("%ecx", syntax),
                   scratch);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, ins->name, scratch);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, ins->name);
}


/* Load address of a string literal (IR_GLOB_STRING). */
static void emit_glob_string(strbuf_t *sb, ir_instr_t *ins,
                             regalloc_t *ra, int x64,
                             asm_syntax_t syntax)
{
    char destb[32];
    char mem[32];
    const char *sfx = x64 ? "q" : "l";
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    char srcbuf[32];
    if (syntax == ASM_INTEL)
        snprintf(srcbuf, sizeof(srcbuf), "%s", ins->name);
    else
        snprintf(srcbuf, sizeof(srcbuf), "$%s", ins->name);
    emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
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
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax)
{
    switch (ins->op) {
    case IR_CONST:
        emit_const(sb, ins, ra, x64, syntax);
        break;
    case IR_LOAD:
        emit_load(sb, ins, ra, x64, syntax);
        break;
    case IR_STORE:
        emit_store(sb, ins, ra, x64, syntax);
        break;
    case IR_LOAD_PARAM:
        emit_load_param(sb, ins, ra, x64, syntax);
        break;
    case IR_STORE_PARAM:
        emit_store_param(sb, ins, ra, x64, syntax);
        break;
    case IR_ADDR:
        emit_addr(sb, ins, ra, x64, syntax);
        break;
    case IR_LOAD_PTR:
        emit_load_ptr(sb, ins, ra, x64, syntax);
        break;
    case IR_STORE_PTR:
        emit_store_ptr(sb, ins, ra, x64, syntax);
        break;
    case IR_LOAD_IDX:
        emit_load_idx(sb, ins, ra, x64, syntax);
        break;
    case IR_STORE_IDX:
        emit_store_idx(sb, ins, ra, x64, syntax);
        break;
    case IR_BFLOAD:
        emit_bfload(sb, ins, ra, x64, syntax);
        break;
    case IR_BFSTORE:
        emit_bfstore(sb, ins, ra, x64, syntax);
        break;
    case IR_ARG:
        emit_arg(sb, ins, ra, x64, syntax);
        break;
    case IR_GLOB_STRING:
    case IR_GLOB_WSTRING:
        emit_glob_string(sb, ins, ra, x64, syntax);
        break;
    case IR_GLOB_VAR:
        break;
    case IR_GLOB_ARRAY:
        break;
    case IR_GLOB_UNION:
        break;
    case IR_GLOB_STRUCT:
        break;
    case IR_GLOB_ADDR:
        break;
    default:
        break;
    }
}


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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "codegen_mem.h"
#include "codegen_loadstore.h"
#include "regalloc_x86.h"
#include "ast.h"

static const char *fmt_stack(char buf[32], const char *name, int x64,
                             asm_syntax_t syntax)
{
    if (strncmp(name, "stack:", 6) != 0)
        return name;
    char *end;
    errno = 0;
    long off = strtol(name + 6, &end, 10);
    if (errno || *end != '\0')
        off = 0;
    if (x64) {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[rbp-%d]", (int)off);
        else
            snprintf(buf, 32, "-%d(%%rbp)", (int)off);
    } else {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[ebp-%d]", (int)off);
        else
            snprintf(buf, 32, "-%d(%%ebp)", (int)off);
    }
    return buf;
}

#define SCRATCH_REG 0

/* The table `mem_emitters` maps IR opcodes to the helpers below. */

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

/* Load the destination value into the scratch register and clear
 * the bit-field position using `clear` as mask. */
static void load_dest_scratch(strbuf_t *sb, const char *sfx,
                              const char *name,
                              unsigned long long clear,
                              int x64, asm_syntax_t syntax)
{
    const char *scratch = regalloc_reg_name(SCRATCH_REG);
    char buf[32];
    const char *src = fmt_stack(buf, name, x64, syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, src);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, src, scratch);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    and%s %s, %llu\n", sfx, scratch, clear);
    else
        strbuf_appendf(sb, "    and%s $%llu, %s\n", sfx, clear, scratch);
}

/* Load the input value, mask it with `mask` and shift by `shift`.  The
 * temporary register %ecx/%rcx is used to hold the intermediate result. */
static void mask_shift_input(strbuf_t *sb, const char *sfx, const char *val,
                             unsigned long long mask, unsigned shift,
                             int x64, asm_syntax_t syntax)
{
    const char *reg = x64 ? fmt_reg("%rcx", syntax)
                          : fmt_reg("%ecx", syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, reg, val);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, val, reg);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    and%s %s, %llu\n", sfx, reg, mask);
    else
        strbuf_appendf(sb, "    and%s $%llu, %s\n", sfx, mask, reg);
    if (shift) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    shl%s %s, %u\n", sfx, reg, shift);
        else
            strbuf_appendf(sb, "    shl%s $%u, %s\n", sfx, shift, reg);
    }
}

/* OR the prepared value in %ecx/%rcx into the scratch register and
 * store the result back to `name`. */
static void write_back_value(strbuf_t *sb, const char *sfx,
                             const char *name, int x64,
                             asm_syntax_t syntax)
{
    const char *scratch = regalloc_reg_name(SCRATCH_REG);
    const char *reg = x64 ? fmt_reg("%rcx", syntax)
                          : fmt_reg("%ecx", syntax);
    strbuf_appendf(sb, "    or%s %s, %s\n", sfx, reg, scratch);
    char buf[32];
    const char *dst = fmt_stack(buf, name, x64, syntax);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, dst, scratch);
    else
        strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, scratch, dst);
}

/* ----------------------------------------------------------------------
 * Load and store emitters
 * ---------------------------------------------------------------------- */

/*
 * Load an immediate constant (IR_CONST).
 *
 * Register allocation expectations:
 *   - `dest` follows the usual load semantics and may be spilled.
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


/*
 * Load a function parameter (IR_LOAD_PARAM).
 *
 * Register allocation expectations:
 *   - `dest` behaves like a normal load destination and may be spilled.
 *   - The parameter value is loaded from the stack based on `imm`.
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
    int off = 8 + (int)ins->imm * (x64 ? 8 : 4);
    char srcbuf[32];
    if (syntax == ASM_INTEL)
        snprintf(srcbuf, sizeof(srcbuf), "[%s+%d]", bp, off);
    else
        snprintf(srcbuf, sizeof(srcbuf), "%d(%s)", off, bp);
    emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
}

/*
 * Store a value to a parameter slot (IR_STORE_PARAM).
 *
 * Register allocation expectations:
 *   - `src1` provides the value to store.
 *   - The destination slot is addressed relative to the frame pointer using
 *     `imm`.
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
    int off = 8 + (int)ins->imm * (x64 ? 8 : 4);
    if (syntax == ASM_INTEL)
        strbuf_appendf(sb, "    mov%s %d(%s), %s\n", sfx,
                       off, bp, loc_str(b1, ra, ins->src1, x64, syntax));
    else
        strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx,
                       loc_str(b1, ra, ins->src1, x64, syntax), off, bp);
}

/*
 * Take the address of a symbol (IR_ADDR).
 *
 * Register allocation expectations:
 *   - `dest` follows normal load semantics and may spill.
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
    const char *name = fmt_stack(srcbuf, ins->name, x64, syntax);
    if (strncmp(ins->name, "stack:", 6) == 0) {
        /* stack address -> lea */
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    lea%s %s, %s\n", sfx, dest, name);
        else
            strbuf_appendf(sb, "    lea%s %s, %s\n", sfx, name, dest);
        if (spill)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, slot, dest);
        return;
    }
    if (syntax == ASM_INTEL)
        snprintf(srcbuf, sizeof(srcbuf), "%s", name);
    else
        snprintf(srcbuf, sizeof(srcbuf), "$%s", name);
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


/* ----------------------------------------------------------------------
 * Bit-field emitters
 * ---------------------------------------------------------------------- */

/*
 * Load a bit-field value (IR_BFLOAD).
 *
 * Register allocation expectations:
 *   - `dest` may be spilled; SCRATCH_REG is used when necessary.
 *   - %ecx/%rcx is used as a temporary when masking and shifting.
 */
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
    char nbuf[32];
    const char *src = fmt_stack(nbuf, ins->name, x64, syntax);
    emit_move_with_spill(sb, sfx, src, dest, slot, 0, syntax);
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

/*
 * Store a value into a bit-field (IR_BFSTORE).
 *
 * Register allocation expectations:
 *   - `src1` is the value to insert into the field.
 *   - Uses SCRATCH_REG and %ecx/%rcx as temporaries when updating the field.
 */
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
    load_dest_scratch(sb, sfx, ins->name, clear, x64, syntax);
    mask_shift_input(sb, sfx,
                     loc_str(bval, ra, ins->src1, x64, syntax),
                     mask, shift, x64, syntax);
    write_back_value(sb, sfx, ins->name, x64, syntax);
}

/*
 * Push an argument (IR_ARG).
 *
 * Register allocation expectations:
 *   - `src1` provides the argument value to push on the stack.
 */
static void emit_arg(strbuf_t *sb, ir_instr_t *ins,
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
    static const char *arg_regs[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    if (x64 && arg_reg_idx < 6 && t != TYPE_FLOAT && t != TYPE_DOUBLE && t != TYPE_LDOUBLE) {
        const char *reg = fmt_reg(arg_regs[arg_reg_idx], syntax);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
        const char *sfx = "q";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, reg, src);
        else
            strbuf_appendf(sb, "    mov%s %s, %s\n", sfx, src, reg);
        arg_reg_idx++;
        return;
    }

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

/* ----------------------------------------------------------------------
 * Global data emitters
 * ---------------------------------------------------------------------- */

/*
 * Load address of a string literal (IR_GLOB_STRING).
 *
 * Register allocation expectations:
 *   - `dest` may be spilled in which case SCRATCH_REG is used.
 */
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

/* Mapping of IR opcodes to emitter helpers */
mem_emit_fn mem_emitters[IR_LABEL + 1] = {
    [IR_CONST] = emit_const,
    [IR_LOAD] = emit_load,
    [IR_STORE] = emit_store,
    [IR_LOAD_PARAM] = emit_load_param,
    [IR_STORE_PARAM] = emit_store_param,
    [IR_ADDR] = emit_addr,
    [IR_LOAD_PTR] = emit_load_ptr,
    [IR_STORE_PTR] = emit_store_ptr,
    [IR_LOAD_IDX] = emit_load_idx,
    [IR_STORE_IDX] = emit_store_idx,
    [IR_BFLOAD] = emit_bfload,
    [IR_BFSTORE] = emit_bfstore,
    [IR_ARG] = emit_arg,
    [IR_GLOB_STRING] = emit_glob_string,
    [IR_GLOB_WSTRING] = emit_glob_string
};


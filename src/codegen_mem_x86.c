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
#define emit_load_idx emit_load_idx_unused
#include "codegen_loadstore.h"
#undef emit_load_idx
#include "regalloc_x86.h"
#include "ast.h"

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

/* Return the temporary register used for bit-field operations. */
static const char *tmp_reg(int x64, asm_syntax_t syntax)
{
    return fmt_reg(x64 ? "%rcx" : "%ecx", syntax);
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
    const char *reg = tmp_reg(x64, syntax);
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
    const char *reg = tmp_reg(x64, syntax);
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
    const char *ext;
    const char *sfx = type_suffix_ext(ins->type, x64, &ext);
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
    if (ext) {
        const char *wsfx = x64 ? "q" : "l";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", ext, dest, srcbuf);
        else
            strbuf_appendf(sb, "    %s %s, %s\n", ext, srcbuf, dest);
        if (spill) {
            if (syntax == ASM_INTEL)
                strbuf_appendf(sb, "    mov%s %s, %s\n", wsfx, slot, dest);
            else
                strbuf_appendf(sb, "    mov%s %s, %s\n", wsfx, dest, slot);
        }
    } else {
        emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
    }
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
    const char *ext;
    const char *sfx = type_suffix_ext(ins->type, x64, &ext);
    (void)ext;
    int off = 8 + (int)ins->imm * (x64 ? 8 : 4);
    const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
    if (sfx[0] == 'b' || sfx[0] == 'w') {
        int reg = (ra && ins->src1 > 0) ? ra->loc[ins->src1] : -1;
        const char *low;
        if (reg >= 0) {
            low = reg_str_sized(reg, sfx[0], x64, syntax);
        } else {
            const char *scratch = reg_str(SCRATCH_REG, syntax);
            const char *wsfx = x64 ? "q" : "l";
            if (syntax == ASM_INTEL)
                strbuf_appendf(sb, "    mov%s %s, %s\n", wsfx, scratch, src);
            else
                strbuf_appendf(sb, "    mov%s %s, %s\n", wsfx, src, scratch);
            low = reg_str_sized(SCRATCH_REG, sfx[0], x64, syntax);
        }
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s [%s+%d], %s\n", sfx, bp, off, low);
        else
            strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx, low, off, bp);
    } else {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    mov%s [%s+%d], %s\n", sfx, bp, off, src);
        else
            strbuf_appendf(sb, "    mov%s %s, %d(%s)\n", sfx, src, off, bp);
    }
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
static void emit_load_idx(strbuf_t *sb, ir_instr_t *ins,
                          regalloc_t *ra, int x64,
                          asm_syntax_t syntax)
{
    char b1[32];
    char destb[32];
    char mem[32];
    const char *ext;
    const char *sfx = type_suffix_ext(ins->type, x64, &ext);
    int spill = (ra && ins->dest > 0 && ra->loc[ins->dest] < 0);
    const char *dest = spill ? reg_str(SCRATCH_REG, syntax)
                             : loc_str(destb, ra, ins->dest, x64, syntax);
    const char *slot = loc_str(mem, ra, ins->dest, x64, syntax);
    int scale = idx_scale(ins, x64);
    char srcbuf[64];
    char basebuf[32];
    const char *base = fmt_stack(basebuf, ins->name, x64, syntax);
    const char *idx = loc_str(b1, ra, ins->src1, x64, syntax);
    if (syntax == ASM_INTEL) {
        char inner[32];
        const char *b = base;
        size_t len = strlen(base);
        if (len >= 2 && base[0] == '[' && base[len - 1] == ']') {
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
    if (ext) {
        const char *wsfx = x64 ? "q" : "l";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", ext, dest, srcbuf);
        else
            strbuf_appendf(sb, "    %s %s, %s\n", ext, srcbuf, dest);
        if (spill) {
            if (syntax == ASM_INTEL)
                strbuf_appendf(sb, "    mov%s %s, %s\n", wsfx, slot, dest);
            else
                strbuf_appendf(sb, "    mov%s %s, %s\n", wsfx, dest, slot);
        }
    } else {
        emit_move_with_spill(sb, sfx, srcbuf, dest, slot, spill, syntax);
    }
}
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
    /* Mask covering the bit-field width. */
    unsigned long long mask = (width == 64) ? 0xffffffffffffffffULL
                                            : ((1ULL << width) - 1ULL);
    char nbuf[32];
    const char *src = fmt_stack(nbuf, ins->name, x64, syntax);
    emit_move_with_spill(sb, sfx, src, dest, slot, 0, syntax);
    if (shift) {
        /* Shift down to align the field with bit 0. */
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    shr%s %s, %u\n", sfx, dest, shift);
        else
            strbuf_appendf(sb, "    shr%s $%u, %s\n", sfx, shift, dest);
    }
    /* Mask off any unrelated bits. */
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
    /* Mask covering the bit-field width. */
    unsigned long long mask = (width == 64) ? 0xffffffffffffffffULL
                                            : ((1ULL << width) - 1ULL);
    /* Clear mask to zero out the destination field. */
    unsigned long long clear = ~((unsigned long long)mask << shift);
    /* Load destination and clear the field bits. */
    load_dest_scratch(sb, sfx, ins->name, clear, x64, syntax);
    /* Prepare the input value for insertion. */
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
        sz = x64 ? 16 : 10;
    static const char *arg_regs[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    static const char *xmm_regs[8] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"};

    if (x64 && t != TYPE_FLOAT && t != TYPE_DOUBLE && t != TYPE_LDOUBLE &&
        arg_reg_idx < 6) {
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

    if (x64 && (t == TYPE_FLOAT || t == TYPE_DOUBLE) && float_reg_idx < 8) {
        const char *reg = fmt_reg(xmm_regs[float_reg_idx], syntax);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
        const char *mov = (t == TYPE_FLOAT) ? "movd" : "movq";
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    %s %s, %s\n", mov, reg, src);
        else
            strbuf_appendf(sb, "    %s %s, %s\n", mov, src, reg);
        float_reg_idx++;
        return;
    }

    if (t == TYPE_FLOAT) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    sub %s, 4\n", sp);
        else
            strbuf_appendf(sb, "    sub $4, %s\n", sp);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
        const char *x0 = fmt_reg("%xmm0", syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movd %s, %s\n", x0, src);
        else
            strbuf_appendf(sb, "    movd %s, %s\n", src, x0);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movss [%s], %s\n", sp, x0);
        else
            strbuf_appendf(sb, "    movss %s, (%s)\n", x0, sp);
    } else if (t == TYPE_DOUBLE) {
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    sub %s, 8\n", sp);
        else
            strbuf_appendf(sb, "    sub $8, %s\n", sp);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
        const char *x0 = fmt_reg("%xmm0", syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movq %s, %s\n", x0, src);
        else
            strbuf_appendf(sb, "    movq %s, %s\n", src, x0);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    movsd [%s], %s\n", sp, x0);
        else
            strbuf_appendf(sb, "    movsd %s, (%s)\n", x0, sp);
    } else if (t == TYPE_LDOUBLE) {
        size_t pad = x64 ? 16 : 10;
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    sub %s, %zu\n", sp, pad);
        else
            strbuf_appendf(sb, "    sub $%zu, %s\n", pad, sp);
        const char *src = loc_str(b1, ra, ins->src1, x64, syntax);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    fld tword ptr %s\n", src);
        else
            strbuf_appendf(sb, "    fldt %s\n", src);
        if (syntax == ASM_INTEL)
            strbuf_appendf(sb, "    fstp tword ptr [%s]\n", sp);
        else
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


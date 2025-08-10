/*
 * Load and store instruction emission helpers.
 *
 * These helpers translate IR load and store operations using register
 * allocation results.  The `x64` flag selects 32- or 64-bit forms.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_LOADSTORE_H
#define VC_CODEGEN_LOADSTORE_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

/* Determine the element size for indexed loads and stores. */
static inline int idx_scale(const ir_instr_t *ins, int x64)
{
    if (ins->imm)
        return (int)ins->imm;
    switch (ins->type) {
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

/*
 * Determine the instruction suffix for loads/stores of the given type and
 * return an optional sign/zero extension instruction for loads of small
 * integers.  The returned suffix selects the memory operand size while the
 * extension instruction loads a byte/word into a full register with the
 * correct signedness.
 */
static inline const char *type_suffix_ext(type_kind_t t, int x64,
                                          const char **ext)
{
    switch (t) {
    case TYPE_CHAR:
        *ext = x64 ? "movsbq" : "movsbl";
        return "b";
    case TYPE_UCHAR:
    case TYPE_BOOL:
        *ext = x64 ? "movzbq" : "movzbl";
        return "b";
    case TYPE_SHORT:
        *ext = x64 ? "movswq" : "movswl";
        return "w";
    case TYPE_USHORT:
        *ext = x64 ? "movzwq" : "movzwl";
        return "w";
    case TYPE_LLONG: case TYPE_ULLONG:
    case TYPE_PTR:
        *ext = NULL;
        return x64 ? "q" : "l";
    default:
        *ext = NULL;
        return "l";
    }
}

/* Return the textual name of register `reg` for the given operand size. */
static inline const char *reg_str_sized(int reg, char sfx, int x64,
                                        asm_syntax_t syntax)
{
    (void)x64;
    static const char *regs8[6]  = {"%al", "%bl", "%cl", "%dl", "%sil", "%dil"};
    static const char *regs16[6] = {"%ax", "%bx", "%cx", "%dx", "%si", "%di"};
    static const char *regs32[6] = {"%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"};
    static const char *regs64[6] = {"%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi"};
    const char *name;
    if (reg < 0 || reg >= 6)
        reg = 0;
    switch (sfx) {
    case 'b': name = regs8[reg]; break;
    case 'w': name = regs16[reg]; break;
    case 'q': name = regs64[reg]; break;
    default:  name = regs32[reg]; break;
    }
    if (syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

void emit_load(strbuf_t *sb, ir_instr_t *ins,
               regalloc_t *ra, int x64,
               asm_syntax_t syntax);

void emit_store(strbuf_t *sb, ir_instr_t *ins,
                regalloc_t *ra, int x64,
                asm_syntax_t syntax);

void emit_load_idx(strbuf_t *sb, ir_instr_t *ins,
                   regalloc_t *ra, int x64,
                   asm_syntax_t syntax);

void emit_store_idx(strbuf_t *sb, ir_instr_t *ins,
                    regalloc_t *ra, int x64,
                    asm_syntax_t syntax);

void emit_load_ptr(strbuf_t *sb, ir_instr_t *ins,
                   regalloc_t *ra, int x64,
                   asm_syntax_t syntax);

void emit_store_ptr(strbuf_t *sb, ir_instr_t *ins,
                    regalloc_t *ra, int x64,
                    asm_syntax_t syntax);

#endif /* VC_CODEGEN_LOADSTORE_H */

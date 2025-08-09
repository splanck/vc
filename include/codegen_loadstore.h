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
        return (int)sizeof(long double);
    case TYPE_DOUBLE_COMPLEX:
        return 2 * (int)sizeof(double);
    case TYPE_LDOUBLE_COMPLEX:
        return 2 * (int)sizeof(long double);
    case TYPE_PTR:
        return x64 ? 8 : 4;
    default:
        return 4;
    }
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

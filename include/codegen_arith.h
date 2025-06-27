/*
 * Arithmetic instruction emission helpers.
 *
 * These functions translate IR arithmetic opcodes using register
 * allocation results.  An `x64` flag selects 32- or 64-bit instruction
 * variants.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_ARITH_H
#define VC_CODEGEN_ARITH_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"

/*
 * Emit assembly for an arithmetic instruction.
 *
 * `ra` provides operand locations and `x64` selects between 32- and
 * 64-bit instruction encodings.
 */
void emit_arith_instr(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64);

#endif /* VC_CODEGEN_ARITH_H */

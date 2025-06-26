/*
 * Arithmetic instruction emission helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_ARITH_H
#define VC_CODEGEN_ARITH_H

#include "strbuf.h"
#include "ir.h"
#include "regalloc.h"

void emit_arith_instr(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64);

#endif /* VC_CODEGEN_ARITH_H */

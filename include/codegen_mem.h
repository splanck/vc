/*
 * Memory instruction emission helpers.
 *
 * These functions lower loads, stores and address computations using the
 * register allocation results.  The `x64` flag selects 32- or 64-bit
 * addressing modes.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_MEM_H
#define VC_CODEGEN_MEM_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

/*
 * Emit assembly for a memory-related instruction.
 *
 * Operands are looked up in `ra` and the `x64` parameter controls the
 * pointer size used in addressing modes.
 */
void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax);

#endif /* VC_CODEGEN_MEM_H */

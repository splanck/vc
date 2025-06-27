/*
 * Branch instruction emission helpers.
 *
 * These functions generate prologue/epilogue code and lower branches and
 * calls.  An `x64` argument determines the x86 variant that is emitted.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_BRANCH_H
#define VC_CODEGEN_BRANCH_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"

/*
 * Emit assembly for branching and function control flow instructions.
 *
 * Uses `ra` for stack frame information and chooses between 32- and
 * 64-bit encodings according to `x64`.
 */
void emit_branch_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64);

#endif /* VC_CODEGEN_BRANCH_H */

/*
 * x86 register name helpers.
 *
 * The register allocator itself only deals with small integer indices.
 * This header provides helpers that map those indices to textual
 * register names understood by the assembler.  Two tables are kept for
 * 32- and 64-bit code generation and `regalloc_set_x86_64` selects the
 * active one.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_REGALLOC_X86_H
#define VC_REGALLOC_X86_H

#include "cli.h"

/*
 * Number of allocatable general purpose registers.
 *
 * Register indices returned by the allocator range from
 * 0 to REGALLOC_NUM_REGS-1.
 */
#define REGALLOC_NUM_REGS 6  /* size of the register pool used by regalloc */

/* Register index used for aggregate return pointers. */
#define REGALLOC_RET_REG 0

/*
 * Return the textual CPU register name for the allocator index `idx`.
 * Indices outside the valid range fall back to the first register of
 * the selected table.
 */
const char *regalloc_reg_name(int idx);

/* Allocate and release temporary XMM registers. */
int regalloc_xmm_acquire(void);
void regalloc_xmm_release(int reg);
void regalloc_xmm_reset(void);

/* Return textual name for an XMM register index. */
const char *regalloc_xmm_name(int idx);

/* Enable or disable 64-bit register naming. */
void regalloc_set_x86_64(int enable);

/* Select assembly syntax flavor for register names. */
void regalloc_set_asm_syntax(asm_syntax_t syntax);

#endif /* VC_REGALLOC_X86_H */

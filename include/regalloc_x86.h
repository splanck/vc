/*
 * x86 register name helpers.
 *
 * These routines translate allocator register indices to the
 * textual names required by the assembler. The same allocator
 * implementation works for both 32- and 64-bit code generation;
 * `regalloc_set_x86_64` selects the appropriate naming.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_REGALLOC_X86_H
#define VC_REGALLOC_X86_H

/*
 * Number of allocatable general purpose registers.
 *
 * Register indices returned by the allocator range from
 * 0 to REGALLOC_NUM_REGS-1.
 */
#define REGALLOC_NUM_REGS 6

/* Return physical register name for given index */
const char *regalloc_reg_name(int idx);

/* Select register naming for 64-bit or 32-bit mode. */
void regalloc_set_x86_64(int enable);

#endif /* VC_REGALLOC_X86_H */

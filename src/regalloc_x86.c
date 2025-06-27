/*
 * Register naming for x86 code generation.
 *
 * The register allocator deals only with abstract indices.  This module
 * translates those indices into the textual names expected by the
 * assembler.  Separate lookup tables exist for 32-bit and 64-bit mode
 * and `regalloc_set_x86_64` toggles between them.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "regalloc_x86.h"

/* Current register naming mode: 0 for 32-bit, 1 for 64-bit. */
static int use_x86_64 = 0;

/* register names for 32-bit mode */
static const char *phys_regs_32[REGALLOC_NUM_REGS] = {
    "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
};

/* register names for 64-bit mode */
static const char *phys_regs_64[REGALLOC_NUM_REGS] = {
    "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi"
};

/*
 * Translate an allocator register index into the textual name of the
 * underlying CPU register.
 *
 * The allocator itself works only with small integer indices.  This
 * function maps those indices to the appropriate register name using
 * either the 32-bit or 64-bit table above.  Out-of-range indices fall
 * back to the first register of the selected table.
 */
const char *regalloc_reg_name(int idx)
{
    const char **regs = use_x86_64 ? phys_regs_64 : phys_regs_32;
    if (idx >= 0 && idx < REGALLOC_NUM_REGS)
        return regs[idx];
    return use_x86_64 ? "%rax" : "%eax";
}

/*
 * Select between 32-bit and 64-bit register naming.
 * A non-zero argument enables 64-bit names.
 */
void regalloc_set_x86_64(int enable)
{
    use_x86_64 = enable ? 1 : 0;
}

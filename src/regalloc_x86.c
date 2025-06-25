/*
 * Register naming for x86 code generation.
 *
 * The register allocator deals only with abstract indices. This
 * module provides the textual names for those indices in either
 * 32- or 64-bit mode. `regalloc_set_x86_64` toggles between the
 * two tables below.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "regalloc_x86.h"

static int use_x86_64 = 0;

static const char *phys_regs_32[REGALLOC_NUM_REGS] = {
    "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
};

static const char *phys_regs_64[REGALLOC_NUM_REGS] = {
    "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi"
};

/* Translate an allocator register index to a physical register name. */
const char *regalloc_reg_name(int idx)
{
    const char **regs = use_x86_64 ? phys_regs_64 : phys_regs_32;
    if (idx >= 0 && idx < REGALLOC_NUM_REGS)
        return regs[idx];
    return use_x86_64 ? "%rax" : "%eax";
}

/* Select between 32-bit and 64-bit register naming. */
void regalloc_set_x86_64(int enable)
{
    use_x86_64 = enable ? 1 : 0;
}

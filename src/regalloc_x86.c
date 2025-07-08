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
/* Assembly syntax flavor for register names. */
static asm_syntax_t current_syntax = ASM_ATT;

/* register names for 32-bit mode */
static const char *phys_regs_32[REGALLOC_NUM_REGS] = {
    "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
};

/* xmm register names */
#define NUM_XMM_REGS 8
static const char *xmm_regs[NUM_XMM_REGS] = {
    "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
};
static int xmm_free_stack[NUM_XMM_REGS];
static int xmm_free_count = 0;

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
    const char *name;
    if (idx >= 0 && idx < REGALLOC_NUM_REGS)
        name = regs[idx];
    else
        name = use_x86_64 ? "%rax" : "%eax";
    if (current_syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

/* Return textual name of an XMM register. */
const char *regalloc_xmm_name(int idx)
{
    const char *name = "%xmm0";
    if (idx >= 0 && idx < NUM_XMM_REGS)
        name = xmm_regs[idx];
    if (current_syntax == ASM_INTEL && name[0] == '%')
        return name + 1;
    return name;
}

/* Reset the pool of available XMM registers. */
void regalloc_xmm_reset(void)
{
    xmm_free_count = NUM_XMM_REGS;
    for (int i = 0; i < NUM_XMM_REGS; i++)
        xmm_free_stack[i] = NUM_XMM_REGS - 1 - i;
}

/* Acquire one free XMM register or return -1 if none available. */
int regalloc_xmm_acquire(void)
{
    if (xmm_free_count == 0)
        return -1;
    return xmm_free_stack[--xmm_free_count];
}

/* Release a previously acquired XMM register. */
void regalloc_xmm_release(int reg)
{
    if (reg < 0 || reg >= NUM_XMM_REGS || xmm_free_count >= NUM_XMM_REGS)
        return;
    xmm_free_stack[xmm_free_count++] = reg;
}

/*
 * Select between 32-bit and 64-bit register naming.
 * A non-zero argument enables 64-bit names.
 */
void regalloc_set_x86_64(int enable)
{
    use_x86_64 = enable ? 1 : 0;
}

/* Set the assembly syntax style used for register names. */
void regalloc_set_asm_syntax(asm_syntax_t syntax)
{
    current_syntax = syntax;
}

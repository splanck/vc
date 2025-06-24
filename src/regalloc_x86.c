#include "regalloc_x86.h"

static int use_x86_64 = 0;

static const char *phys_regs_32[REGALLOC_NUM_REGS] = {
    "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
};

static const char *phys_regs_64[REGALLOC_NUM_REGS] = {
    "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi"
};

const char *regalloc_reg_name(int idx)
{
    const char **regs = use_x86_64 ? phys_regs_64 : phys_regs_32;
    if (idx >= 0 && idx < REGALLOC_NUM_REGS)
        return regs[idx];
    return use_x86_64 ? "%rax" : "%eax";
}

void regalloc_set_x86_64(int enable)
{
    use_x86_64 = enable ? 1 : 0;
}

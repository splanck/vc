#ifndef VC_REGALLOC_X86_H
#define VC_REGALLOC_X86_H

/* Number of allocatable general purpose registers */
#define REGALLOC_NUM_REGS 6

/* Return physical register name for given index */
const char *regalloc_reg_name(int idx);

/* Select register naming for 64-bit or 32-bit mode. */
void regalloc_set_x86_64(int enable);

#endif /* VC_REGALLOC_X86_H */

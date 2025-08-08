#include "codegen_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Current argument stack size for the active call. */
size_t arg_stack_bytes = 0;
/* Next argument register index used for x86-64 calls. */
int arg_reg_idx = 0;
/* Next XMM argument register index used for x86-64 calls. */
int float_reg_idx = 0;

/* Architecture specific memory emitters. */
extern mem_emit_fn mem_emitters[];

/*
 * Dispatch a single memory related IR instruction using the
 * architecture specific emitter table. This function is used by
 * codegen.c after register allocation has assigned locations to IR
 * values.
 */
void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax)
{
    if (!ins || ins->op < 0 || ins->op > IR_LABEL)
        return;
    mem_emit_fn fn = mem_emitters[ins->op];
    if (fn)
        fn(sb, ins, ra, x64, syntax);
}

/* Convert "stack:offset" names to frame-pointer relative operands. */
const char *fmt_stack(char buf[32], const char *name, int x64,
                      asm_syntax_t syntax)
{
    if (strncmp(name, "stack:", 6) != 0)
        return name;
    char *end;
    errno = 0;
    long off = strtol(name + 6, &end, 10);
    if (errno || *end != '\0')
        off = 0;
    if (x64) {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[rbp-%d]", (int)off);
        else
            snprintf(buf, 32, "-%d(%%rbp)", (int)off);
    } else {
        if (syntax == ASM_INTEL)
            snprintf(buf, 32, "[ebp-%d]", (int)off);
        else
            snprintf(buf, 32, "-%d(%%ebp)", (int)off);
    }
    return buf;
}

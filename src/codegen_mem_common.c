#include "codegen_mem.h"

/* Current argument stack size for the active call. */
size_t arg_stack_bytes = 0;
/* Next argument register index used for x86-64 calls. */
int arg_reg_idx = 0;

/* Architecture specific memory emitters. */
typedef void (*mem_emit_fn)(strbuf_t *, ir_instr_t *, regalloc_t *, int,
                            asm_syntax_t);
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

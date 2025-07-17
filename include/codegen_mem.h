/*
 * Memory instruction emission helpers.
 *
 * These functions lower loads, stores and address computations using the
 * register allocation results.  The `x64` flag selects 32- or 64-bit
 * addressing modes.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_MEM_H
#define VC_CODEGEN_MEM_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

/* Architecture specific emitter function type. */
typedef void (*mem_emit_fn)(strbuf_t *, ir_instr_t *, regalloc_t *, int,
                            asm_syntax_t);

/* Table of emitter callbacks defined by the target backend. */
extern mem_emit_fn mem_emitters[];

/*
 * Emit assembly for a single memory-related instruction.
 *
 * Operands are looked up in `ra` and the `x64` parameter controls the
 * pointer size used in addressing modes.  `regalloc` must populate
 * `ra->loc` for all values referenced by `ins`.  `codegen.c` invokes
 * this helper after performing register allocation.
 */
void emit_memory_instr(strbuf_t *sb, ir_instr_t *ins,
                       regalloc_t *ra, int x64,
                       asm_syntax_t syntax);

/* bytes pushed for the current argument list */
extern size_t arg_stack_bytes;
extern int arg_reg_idx;

const char *fmt_stack(char buf[32], const char *name, int x64,
                      asm_syntax_t syntax);

#endif /* VC_CODEGEN_MEM_H */

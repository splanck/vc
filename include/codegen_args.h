/*
 * Argument passing helpers.
 *
 * Provides routines for pushing function arguments onto the stack
 * and tracks the total bytes pushed for the current call.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_ARGS_H
#define VC_CODEGEN_ARGS_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

void emit_arg(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax);

extern size_t arg_stack_bytes;

#endif /* VC_CODEGEN_ARGS_H */

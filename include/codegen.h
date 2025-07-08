/*
 * Interfaces for generating assembly from IR.
 *
 * The public code generation API converts the intermediate representation
 * into x86 assembly.  Each routine accepts a flag selecting 32- or
 * 64-bit output so that the caller may target either variant of the
 * architecture.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_H
#define VC_CODEGEN_H

#include <stdio.h>
#include "ir_core.h"
#include "cli.h"

/*
 * Emit the full x86 assembly for `ir` to `out`.
 *
 * Global directives such as string literals are written first under a
 * `.data` section.  The instruction stream is then lowered via the
 * register allocator and printed after an optional `.text` header.
 * Passing a non-zero `x86_64` enables 64-bit register names and pointer
 * sizes.
 */
void codegen_emit_x86(FILE *out, ir_builder_t *ir, int x86_64,
                      asm_syntax_t syntax);

/*
 * Convert the IR to an assembly string.
 *
 * Only the instruction stream is returned; global data is omitted.  The
 * caller owns the returned buffer and must free it.  The `x86_64` flag
 * selects whether 32- or 64-bit mnemonics are produced.
 */
char *codegen_ir_to_string(ir_builder_t *ir, int x86_64,
                           asm_syntax_t syntax);

/*
 * Set whether function symbols should be exported.
 *
 * When enabled, the generated assembly marks each function with `.globl`
 * so that it is visible to the linker.
 */
void codegen_set_export(int flag);

/* Toggle emission of .file and .loc directives */
void codegen_set_debug(int flag);

/* Toggle emission of DWARF sections */
void codegen_set_dwarf(int flag);

/*
 * These flags are global variables defined in codegen.c so that other
 * code generation modules can inspect them.
 */

#endif /* VC_CODEGEN_H */

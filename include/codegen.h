/*
 * Interfaces for generating assembly from IR.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CODEGEN_H
#define VC_CODEGEN_H

#include <stdio.h>
#include "ir.h"

/*
 * Emit the full x86 assembly for `ir` to `out`.
 *
 * The function first outputs any IR global directives under a `.data`
 * section. The instruction stream is then translated to x86 using the
 * register allocator and written after an optional `.text` directive.
 * Pass a non-zero `x86_64` to produce 64-bit instructions.
 */
void codegen_emit_x86(FILE *out, ir_builder_t *ir, int x86_64);

/*
 * Convert the IR to an assembly string.
 *
 * Only the instruction stream is returned. Global data directives are
 * omitted. The caller owns the returned buffer and must free it. Use
 * `x86_64` to choose between 32- and 64-bit code generation.
 */
char *codegen_ir_to_string(ir_builder_t *ir, int x86_64);

/* Set whether function symbols should be exported */
void codegen_set_export(int flag);

#endif /* VC_CODEGEN_H */

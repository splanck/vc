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

/* Emit x86 assembly for the given IR builder to the specified stream.
 * Set `x86_64` to a non-zero value to generate 64-bit code. */
void codegen_emit_x86(FILE *out, ir_builder_t *ir, int x86_64);

/* Generate an assembly string for the given IR. The caller must free the
 * returned buffer. Pass `x86_64` to select 64-bit output. */
char *codegen_ir_to_string(ir_builder_t *ir, int x86_64);

#endif /* VC_CODEGEN_H */

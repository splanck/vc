#ifndef VC_CODEGEN_H
#define VC_CODEGEN_H

#include <stdio.h>
#include "ir.h"

/* Emit x86 assembly for the given IR builder to the specified stream. */
void codegen_emit_x86(FILE *out, ir_builder_t *ir);

/* Generate an assembly string for the given IR. The caller must free the
 * returned buffer. */
char *codegen_ir_to_string(ir_builder_t *ir);

#endif /* VC_CODEGEN_H */

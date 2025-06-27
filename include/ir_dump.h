/*
 * Helpers to convert IR to a string.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_DUMP_H
#define VC_IR_DUMP_H

#include "ir_core.h"

/*
 * Generate a human readable string for the IR. Each instruction is
 * printed on a separate line starting with the opcode name followed by
 * the fields "dest", "src1", "src2", "imm", "name" and "data".  For
 * IR_GLOB_ARRAY the count is printed instead of the generic fields.
 * Caller must free the returned buffer.
 */
char *ir_to_string(ir_builder_t *b);

#endif /* VC_IR_DUMP_H */

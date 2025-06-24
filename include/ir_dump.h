/*
 * Helpers to convert IR to a string.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_IR_DUMP_H
#define VC_IR_DUMP_H

#include "ir.h"

/* Generate a string representation of the IR. Caller must free. */
char *ir_to_string(ir_builder_t *b);

#endif /* VC_IR_DUMP_H */

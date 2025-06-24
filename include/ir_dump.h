#ifndef VC_IR_DUMP_H
#define VC_IR_DUMP_H

#include "ir.h"

/* Generate a string representation of the IR. Caller must free. */
char *ir_to_string(ir_builder_t *b);

#endif /* VC_IR_DUMP_H */

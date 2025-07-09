#ifndef VC_CODEGEN_ARITH_FLOAT_H
#define VC_CODEGEN_ARITH_FLOAT_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

void emit_cast(strbuf_t *sb, ir_instr_t *ins,
               regalloc_t *ra, int x64,
               asm_syntax_t syntax);
void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins,
                           regalloc_t *ra, int x64, const char *op,
                           asm_syntax_t syntax);

#endif /* VC_CODEGEN_ARITH_FLOAT_H */

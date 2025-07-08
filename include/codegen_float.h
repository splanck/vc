#ifndef VC_CODEGEN_FLOAT_H
#define VC_CODEGEN_FLOAT_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

/* Basic single-precision floating point operations */
void emit_float_binop(strbuf_t *sb, ir_instr_t *ins,
                      regalloc_t *ra, int x64, const char *op,
                      asm_syntax_t syntax);

/* Complex arithmetic helpers */
void emit_cplx_addsub(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                      int x64, const char *op, asm_syntax_t syntax);
void emit_cplx_mul(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                   int x64, asm_syntax_t syntax);
void emit_cplx_div(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                   int x64, asm_syntax_t syntax);

#endif /* VC_CODEGEN_FLOAT_H */

#ifndef VC_CODEGEN_ARITH_INT_H
#define VC_CODEGEN_ARITH_INT_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

void emit_ptr_add(strbuf_t *sb, ir_instr_t *ins,
                  regalloc_t *ra, int x64,
                  asm_syntax_t syntax);
void emit_ptr_diff(strbuf_t *sb, ir_instr_t *ins,
                   regalloc_t *ra, int x64,
                   asm_syntax_t syntax);
void emit_int_arith(strbuf_t *sb, ir_instr_t *ins,
                    regalloc_t *ra, int x64, const char *op,
                    asm_syntax_t syntax);
void emit_div(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax);
void emit_mod(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax);
void emit_shift(strbuf_t *sb, ir_instr_t *ins,
                regalloc_t *ra, int x64, const char *op,
                asm_syntax_t syntax);
void emit_bitwise(strbuf_t *sb, ir_instr_t *ins,
                  regalloc_t *ra, int x64, const char *op,
                  asm_syntax_t syntax);
void emit_cmp(strbuf_t *sb, ir_instr_t *ins,
              regalloc_t *ra, int x64,
              asm_syntax_t syntax);
void emit_logand(strbuf_t *sb, ir_instr_t *ins,
                 regalloc_t *ra, int x64,
                 asm_syntax_t syntax);
void emit_logor(strbuf_t *sb, ir_instr_t *ins,
                regalloc_t *ra, int x64,
                asm_syntax_t syntax);

#endif /* VC_CODEGEN_ARITH_INT_H */

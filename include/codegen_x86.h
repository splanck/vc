#ifndef VC_CODEGEN_X86_H
#define VC_CODEGEN_X86_H

#include "strbuf.h"
#include "ir_core.h"
#include "regalloc.h"
#include "cli.h"

const char *x86_reg_str(int reg, asm_syntax_t syntax);
const char *x86_fmt_reg(const char *name, asm_syntax_t syntax);
const char *x86_loc_str(char buf[32], regalloc_t *ra, int id, int x64,
                        asm_syntax_t syntax);

void x86_emit_mov(strbuf_t *sb, const char *sfx,
                  const char *src, const char *dest,
                  asm_syntax_t syntax);
void x86_emit_op(strbuf_t *sb, const char *op, const char *sfx,
                 const char *src, const char *dest,
                 asm_syntax_t syntax);

#endif /* VC_CODEGEN_X86_H */

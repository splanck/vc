#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_arith_int.h"
#include "strbuf.h"
#include "regalloc.h"

/* Minimal stubs to satisfy linker requirements. */
const char *label_format_suffix(char buf[32], const char *prefix, int id,
                                const char *suffix) {
    (void)buf; (void)prefix; (void)id; (void)suffix; return "";
}
void emit_float_binop(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                      asm_syntax_t syntax) {
    (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax;
}
void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                           int x64, asm_syntax_t syntax) {
    (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax;
}
void emit_cplx_addsub(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                      asm_syntax_t syntax, const char *op) {
    (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; (void)op;
}
void emit_cplx_mul(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                   asm_syntax_t syntax) {
    (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax;
}
void emit_cplx_div(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                   asm_syntax_t syntax) {
    (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax;
}
void emit_cast(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
               asm_syntax_t syntax) {
    (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax;
}
int label_next_id(void) { return 0; }
void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

int main(void) {
    int locs[4] = {0, 0, 1, 2};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ins.op = IR_PTR_DIFF;
    ins.dest = 3;
    ins.src1 = 1;
    ins.src2 = 2;
    ins.type = TYPE_INT;
    ins.imm = 0; /* element size */

    strbuf_init(&sb);
    emit_ptr_diff(&sb, &ins, &ra, 0, ASM_ATT);
    if (strstr(sb.data, "idiv") || strstr(sb.data, "sar") || !strstr(sb.data, "xor")) {
        printf("ATT: unexpected output: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_ptr_diff(&sb, &ins, &ra, 0, ASM_INTEL);
    if (strstr(sb.data, "idiv") || strstr(sb.data, "sar") || !strstr(sb.data, "xor")) {
        printf("Intel: unexpected output: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("ptr diff zero tests passed\n");
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_arith_int.h"
#include "strbuf.h"
#include "regalloc.h"

/* Stubs required by codegen_arith_int.c */
const char *label_format_suffix(char buf[32], const char *prefix, int id,
                                const char *suffix) { (void)buf; (void)prefix; (void)id; (void)suffix; return ""; }
void emit_float_binop(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                      asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_long_float_binop(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra,
                           int x64, asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_cplx_addsub(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                      asm_syntax_t syntax, const char *op) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; (void)op; }
void emit_cplx_mul(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                   asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_cplx_div(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
                   asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
void emit_cast(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64,
               asm_syntax_t syntax) { (void)sb; (void)ins; (void)ra; (void)x64; (void)syntax; }
int label_next_id(void) { return 0; }
void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int contains(const char *s, const char *sub) { return strstr(s, sub) != NULL; }

int main(void) {
    int locs[4] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ins.src1 = 1;
    ins.src2 = 2;
    ins.dest = 3;

    ra.loc[1] = 0; /* %eax */
    ra.loc[2] = 1; /* %ebx */
    ra.loc[3] = 2; /* %ecx */

    struct { ir_op_t op; const char *signed_cc; const char *unsigned_cc; } tests[] = {
        { IR_CMPLT, "setl",  "setb"  },
        { IR_CMPLE, "setle", "setbe" },
        { IR_CMPGT, "setg",  "seta"  },
        { IR_CMPGE, "setge", "setae" },
    };

    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        ins.op = tests[i].op;

        ins.type = TYPE_INT;
        strbuf_init(&sb);
        emit_cmp(&sb, &ins, &ra, 0, ASM_ATT);
        if (!contains(sb.data, tests[i].signed_cc)) {
            printf("signed %zu failed: %s\n", i, sb.data);
            return 1;
        }
        strbuf_free(&sb);

        ins.type = TYPE_UINT;
        strbuf_init(&sb);
        emit_cmp(&sb, &ins, &ra, 0, ASM_ATT);
        if (!contains(sb.data, tests[i].unsigned_cc)) {
            printf("unsigned %zu failed: %s\n", i, sb.data);
            return 1;
        }
        strbuf_free(&sb);
    }

    printf("cmp signed tests passed\n");
    return 0;
}

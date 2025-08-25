#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "codegen_arith_int.h"
#include "strbuf.h"
#include "regalloc_x86.h"

/* Stub implementations required by codegen_arith_int.c */
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
    int locs[4] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins = { .op = IR_SHL, .src1 = 1, .src2 = 2, .dest = 3, .type = TYPE_INT };
    strbuf_t sb;
    char asmfile[L_tmpnam];
    char obj[sizeof asmfile + 2];
    char cmd[sizeof asmfile + sizeof obj + 10];

    regalloc_set_asm_syntax(ASM_ATT);
    regalloc_set_x86_64(0);
    ra.loc[1] = 3; /* %edx */
    ra.loc[2] = 1; /* %ebx */
    ra.loc[3] = 2; /* %ecx */

    strbuf_init(&sb);
    emit_shift(&sb, &ins, &ra, 0, "shl", ASM_ATT);

    if (!tmpnam(asmfile)) {
        perror("tmpnam");
        strbuf_free(&sb);
        return 1;
    }
    FILE *f = fopen(asmfile, "w");
    fputs(sb.data, f);
    fclose(f);
    strbuf_free(&sb);

    snprintf(obj, sizeof obj, "%s.o", asmfile);
    snprintf(cmd, sizeof cmd, "as %s -o %s", asmfile, obj);
    int ret = system(cmd);
    unlink(asmfile);
    unlink(obj);
    if (ret != 0) {
        printf("as failed\n");
        return 1;
    }
    printf("32-bit assembly assembled successfully\n");
    return 0;
}

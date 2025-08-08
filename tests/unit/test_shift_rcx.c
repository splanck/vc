#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_arith_int.h"
#include "strbuf.h"
#include "regalloc_x86.h"

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
    ir_instr_t ins = { .op = IR_SHL, .src1 = 1, .src2 = 2, .dest = 3 };
    strbuf_t sb;
    const char *out;
    int fail = 0;

    regalloc_set_asm_syntax(ASM_ATT);

    /* 32-bit shift with destination in %ecx */
    regalloc_set_x86_64(0);
    ins.type = TYPE_INT;
    ra.loc[1] = 3; /* %edx */
    ra.loc[2] = 1; /* %ebx */
    ra.loc[3] = 2; /* %ecx */
    strbuf_init(&sb);
    emit_shift(&sb, &ins, &ra, 0, "shl", ASM_ATT);
    out = sb.data;
    if (!contains(out, "movl %edx, %eax") ||
        !contains(out, "movl %ebx, %ecx") ||
        !contains(out, "shll %cl, %eax\n    movl %eax, %ecx") ||
        contains(out, "movl %edx, %ecx")) {
        printf("32-bit failed: %s\n", out);
        fail = 1;
    }
    strbuf_free(&sb);

    /* 64-bit shift with destination in %rcx */
    regalloc_set_x86_64(1);
    ins.type = TYPE_LONG;
    ra.loc[1] = 3; /* %rdx */
    ra.loc[2] = 1; /* %rbx */
    ra.loc[3] = 2; /* %rcx */
    strbuf_init(&sb);
    emit_shift(&sb, &ins, &ra, 1, "shl", ASM_ATT);
    out = sb.data;
    if (!contains(out, "movq %rdx, %rax") ||
        !contains(out, "movq %rbx, %rcx") ||
        !contains(out, "shlq %cl, %rax\n    movq %rax, %rcx") ||
        contains(out, "movq %rdx, %rcx")) {
        printf("64-bit failed: %s\n", out);
        fail = 1;
    }
    strbuf_free(&sb);

    if (!fail)
        printf("emit_shift rcx tests passed\n");
    return fail;
}

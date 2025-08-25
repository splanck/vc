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
    const char *out;

    ins.op = IR_DIV;
    ins.dest = 3;
    ins.src1 = 1;
    ins.src2 = 2;
    ins.type = TYPE_INT;

    ra.loc[1] = 1;  /* %ebx */
    ra.loc[2] = 2;  /* %ecx */
    ra.loc[3] = -1; /* spilled destination */

    /* Signed division */
    strbuf_init(&sb);
    emit_div(&sb, &ins, &ra, 0, ASM_ATT);
    out = sb.data;
    if (!contains(out, "idivl %ecx") ||
        !contains(out, "movl %eax, -4(%ebp)")) {
        printf("div spill ATT failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_div(&sb, &ins, &ra, 0, ASM_INTEL);
    out = sb.data;
    if (!contains(out, "idivl ecx") ||
        !contains(out, "mov [ebp-4], eax")) {
        printf("div spill Intel failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    /* Unsigned division */
    ins.type = TYPE_UINT;
    strbuf_init(&sb);
    emit_div(&sb, &ins, &ra, 0, ASM_ATT);
    out = sb.data;
    if (!contains(out, "divl %ecx") ||
        !contains(out, "xorl %edx, %edx") ||
        !contains(out, "movl %eax, -4(%ebp)")) {
        printf("div spill unsigned ATT failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_div(&sb, &ins, &ra, 0, ASM_INTEL);
    out = sb.data;
    if (!contains(out, "divl ecx") ||
        !contains(out, "xor edx, edx") ||
        !contains(out, "mov [ebp-4], eax")) {
        printf("div spill unsigned Intel failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    /* Signed modulus with register destination */
    ins.op = IR_MOD;
    ins.type = TYPE_INT;
    ra.loc[3] = 1;  /* %ebx holds result */
    strbuf_init(&sb);
    emit_mod(&sb, &ins, &ra, 0, ASM_ATT);
    out = sb.data;
    if (!contains(out, "idivl %ecx") ||
        !contains(out, "movl %edx, %ebx")) {
        printf("mod ATT failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_mod(&sb, &ins, &ra, 0, ASM_INTEL);
    out = sb.data;
    if (!contains(out, "idivl ecx") ||
        !contains(out, "mov ebx, edx")) {
        printf("mod Intel failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    /* Unsigned modulus */
    ins.type = TYPE_UINT;
    strbuf_init(&sb);
    emit_mod(&sb, &ins, &ra, 0, ASM_ATT);
    out = sb.data;
    if (!contains(out, "divl %ecx") ||
        !contains(out, "xorl %edx, %edx") ||
        !contains(out, "movl %edx, %ebx")) {
        printf("mod unsigned ATT failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_mod(&sb, &ins, &ra, 0, ASM_INTEL);
    out = sb.data;
    if (!contains(out, "divl ecx") ||
        !contains(out, "xor edx, edx") ||
        !contains(out, "mov ebx, edx")) {
        printf("mod unsigned Intel failed: %s\n", out);
        return 1;
    }
    strbuf_free(&sb);

    printf("div tests passed\n");
    return 0;
}

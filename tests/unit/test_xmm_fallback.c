#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_float.h"
#include "strbuf.h"
#include "regalloc_x86.h"

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int has_spill(const char *s) {
    return strstr(s, "movaps") != NULL;
}

int main(void) {
    ir_instr_t ins = {0};
    strbuf_t sb;

    regalloc_set_asm_syntax(ASM_ATT);

    /* Exhaust XMM registers and test float binop fallback */
    regalloc_xmm_reset();
    while (regalloc_xmm_acquire() >= 0)
        ;
    strbuf_init(&sb);
    emit_float_binop(&sb, &ins, NULL, 0, "addss", ASM_ATT);
    if (!has_spill(sb.data)) {
        printf("float binop spill missing: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Exhaust again and test complex multiply fallback */
    regalloc_xmm_reset();
    while (regalloc_xmm_acquire() >= 0)
        ;
    strbuf_init(&sb);
    emit_cplx_mul(&sb, &ins, NULL, 0, ASM_ATT);
    if (!has_spill(sb.data)) {
        printf("complex mul spill missing: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("xmm spill tests passed\n");
    return 0;
}

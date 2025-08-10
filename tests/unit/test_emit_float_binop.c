#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_float.h"
#include "strbuf.h"
#include "regalloc_x86.h"

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int check(const char *s, const char *sub, const char *msg) {
    if (!strstr(s, sub)) {
        printf("%s failed: %s\n", msg, s);
        return 1;
    }
    return 0;
}

int main(void) {
    ir_instr_t ins = {0};
    strbuf_t sb;
    int locs[4] = {0, -1, -2, -3};
    regalloc_t ra = { .loc = locs };
    int fail = 0;

    ins.src1 = 1;
    ins.src2 = 2;
    ins.dest = 3;

    strbuf_init(&sb);
    regalloc_set_x86_64(1);

    /* ATT syntax */
    regalloc_xmm_reset();
    regalloc_set_asm_syntax(ASM_ATT);
    emit_float_binop(&sb, &ins, &ra, 1, "addss", ASM_ATT);
    fail |= check(sb.data, "addss %xmm0, %xmm1", "ATT op order");
    fail |= check(sb.data, "movss %xmm1, -24(%rbp)", "ATT dest register");
    sb.len = 0; if (sb.data) sb.data[0] = '\0';

    /* Intel syntax */
    regalloc_xmm_reset();
    regalloc_set_asm_syntax(ASM_INTEL);
    emit_float_binop(&sb, &ins, &ra, 1, "addss", ASM_INTEL);
    fail |= check(sb.data, "addss xmm0, xmm1", "Intel op order");
    fail |= check(sb.data, "movss [rbp-24], xmm0", "Intel dest register");

    strbuf_free(&sb);
    if (fail) return 1;
    printf("emit_float_binop tests passed\n");
    return 0;
}

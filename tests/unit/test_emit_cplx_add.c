#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_float.h"
#include "strbuf.h"
#include "regalloc_x86.h"

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

int main(void) {
    ir_instr_t ins = {0};
    strbuf_t sb;
    int fail = 0;
    int locs[4] = {0, -1, -2, -3};
    regalloc_t ra = { .loc = locs };

    ins.src1 = 1;
    ins.src2 = 2;
    ins.dest = 3;

    strbuf_init(&sb);

    /* ATT syntax */
    regalloc_set_x86_64(1);
    regalloc_xmm_reset();
    regalloc_set_asm_syntax(ASM_ATT);
    emit_cplx_addsub(&sb, &ins, &ra, 1, "add", ASM_ATT);
    int count = 0;
    for (char *p = sb.data; (p = strstr(p, "addsd %xmm1, %xmm0")); p += 1)
        count++;
    if (count != 2) {
        printf("ATT unexpected output: %s\n", sb.data);
        fail = 1;
    }
    sb.len = 0; if (sb.data) sb.data[0] = '\0';

    /* Intel syntax */
    regalloc_xmm_reset();
    regalloc_set_asm_syntax(ASM_INTEL);
    emit_cplx_addsub(&sb, &ins, &ra, 1, "add", ASM_INTEL);
    count = 0;
    for (char *p = sb.data; (p = strstr(p, "addsd xmm0, xmm1")); p += 1)
        count++;
    if (count != 2) {
        printf("Intel unexpected output: %s\n", sb.data);
        fail = 1;
    }

    strbuf_free(&sb);
    if (fail)
        return 1;
    printf("emit_cplx_add tests passed\n");
    return 0;
}

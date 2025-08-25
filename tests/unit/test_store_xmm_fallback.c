#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc_x86.h"

/* Minimal fmt_stack to interpret "stack:" operands. */
const char *fmt_stack(char buf[32], const char *name, int x64, asm_syntax_t syntax) {
    if (strncmp(name, "stack:", 6) != 0)
        return name;
    long off = strtol(name + 6, NULL, 10);
    if (syntax == ASM_INTEL)
        snprintf(buf, 32, x64 ? "[rbp-%ld]" : "[ebp-%ld]", off);
    else
        snprintf(buf, 32, x64 ? "-%ld(%%rbp)" : "-%ld(%%ebp)", off);
    return buf;
}

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int has_fail(const char *s) {
    return strstr(s, "XMM register allocation failed") != NULL;
}

int main(void) {
    ir_instr_t ins = {0};
    strbuf_t sb;
    int locs[2] = {0};
    regalloc_t ra = { .loc = locs };

    regalloc_set_asm_syntax(ASM_ATT);

    /* Exhaust XMM registers and test store fallback */
    regalloc_xmm_reset();
    while (regalloc_xmm_acquire() >= 0)
        ;

    locs[1] = -1; /* source on stack */
    ins.op = IR_STORE;
    ins.src1 = 1;
    ins.name = "stack:16"; /* destination */
    ins.type = TYPE_DOUBLE_COMPLEX; /* 16 bytes */

    strbuf_init(&sb);
    emit_store(&sb, &ins, &ra, 1, ASM_ATT);
    if (!has_fail(sb.data)) {
        printf("store xmm fallback missing: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("store xmm fallback test passed\n");
    return 0;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc.h"

/* Minimal stubs required by codegen helpers. */
const char *fmt_stack(char buf[32], const char *name, int x64,
                      asm_syntax_t syntax) {
    (void)buf; (void)x64; (void)syntax;
    return name;
}

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int check(type_kind_t t, const char *expect) {
    int locs[3] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    /* Load test */
    ra.loc[1] = 0;
    ins.op = IR_LOAD;
    ins.dest = 1;
    ins.name = "var";
    ins.type = t;
    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 0, ASM_ATT);
    if (!strstr(sb.data, expect)) {
        printf("load ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 0, ASM_INTEL);
    if (!strstr(sb.data, expect)) {
        printf("load Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Store test */
    ra.loc[1] = 0;
    ins.op = IR_STORE;
    ins.src1 = 1;
    ins.name = "var";
    ins.type = t;
    strbuf_init(&sb);
    emit_store(&sb, &ins, &ra, 0, ASM_ATT);
    if (!strstr(sb.data, expect)) {
        printf("store ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store(&sb, &ins, &ra, 0, ASM_INTEL);
    if (!strstr(sb.data, expect)) {
        printf("store Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    return 0;
}

int main(void) {
    if (check(TYPE_CHAR, "movb") || check(TYPE_SHORT, "movw"))
        return 1;
    printf("load/store char/short tests passed\n");
    return 0;
}

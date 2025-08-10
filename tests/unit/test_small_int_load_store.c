#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc.h"

/* Minimal stubs for helpers used by codegen modules. */
const char *fmt_stack(char buf[32], const char *name, int x64, asm_syntax_t syntax) {
    (void)buf; (void)x64; (void)syntax; return name;
}
void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int contains(const char *s, const char *sub) {
    return strstr(s, sub) != NULL;
}

int main(void) {
    int locs[3] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    /* destination/source register index 1 -> %eax/%rax */
    ra.loc[1] = 0;

    /* Signed char load */
    ins.op = IR_LOAD;
    ins.dest = 1;
    ins.name = "c";
    ins.type = TYPE_CHAR;
    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 0, ASM_ATT);
    if (!contains(sb.data, "movsbl")) {
        printf("load char failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Unsigned char load */
    ins.type = TYPE_UCHAR;
    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 0, ASM_ATT);
    if (!contains(sb.data, "movzbl")) {
        printf("load uchar failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Signed short load */
    ins.type = TYPE_SHORT;
    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 0, ASM_ATT);
    if (!contains(sb.data, "movswl")) {
        printf("load short failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Unsigned short load */
    ins.type = TYPE_USHORT;
    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 0, ASM_ATT);
    if (!contains(sb.data, "movzwl")) {
        printf("load ushort failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Char store */
    ins.op = IR_STORE;
    ins.src1 = 1;
    ins.name = "c";
    ins.type = TYPE_CHAR;
    strbuf_init(&sb);
    emit_store(&sb, &ins, &ra, 0, ASM_ATT);
    if (!(contains(sb.data, "movb") && contains(sb.data, "%al"))) {
        printf("store char failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Short store */
    ins.type = TYPE_SHORT;
    strbuf_init(&sb);
    emit_store(&sb, &ins, &ra, 0, ASM_ATT);
    if (!(contains(sb.data, "movw") && contains(sb.data, "%ax"))) {
        printf("store short failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 64-bit signed char load */
    ins.type = TYPE_CHAR;
    strbuf_init(&sb);
    emit_load(&sb, &ins, &ra, 1, ASM_ATT);
    if (!contains(sb.data, "movsbq")) {
        printf("load char x64 failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("small int load/store tests passed\n");
    return 0;
}

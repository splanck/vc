#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc.h"

/* Provide minimal stubs required by codegen helpers. */
const char *fmt_stack(char buf[32], const char *name, int x64,
                      asm_syntax_t syntax) {
    (void)x64;
    if (syntax == ASM_INTEL) {
        snprintf(buf, 32, "[%s]", name);
        return buf;
    }
    return name;
}

void *vc_alloc_or_exit(size_t sz) { return malloc(sz); }
void *vc_realloc_or_exit(void *p, size_t sz) { return realloc(p, sz); }

static int has_invalid(const char *s) {
    return strstr(s, "[[") || strstr(s, "((");
}

int main(void) {
    int locs[3] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    /* index in stack slot, value in register */
    ra.loc[1] = -1; /* spilled index */
    ra.loc[2] = 1;  /* value register */
    ins.op = IR_STORE_IDX;
    ins.src1 = 1;
    ins.src2 = 2;
    ins.name = "base";
    ins.type = TYPE_PTR;
    ins.imm = 4;

    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 0, ASM_ATT);
    if (has_invalid(sb.data) || !strstr(sb.data, "(,%eax,")) {
        printf("store idx spill ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 0, ASM_INTEL);
    if (has_invalid(sb.data) || !strstr(sb.data, "+eax*")) {
        printf("store idx spill Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("store idx spill tests passed\n");
    return 0;
}

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

    /* index in stack slot, destination in register */
    ra.loc[1] = -1; /* spilled index */
    ra.loc[2] = 1;  /* destination register */
    ins.op = IR_LOAD_IDX;
    ins.dest = 2;
    ins.src1 = 1;
    ins.name = "base";
    ins.type = TYPE_PTR;
    ins.imm = 4;

    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 0, ASM_ATT);
    if (has_invalid(sb.data) || !strstr(sb.data, "(,%eax,")) {
        printf("load idx spill ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 0, ASM_INTEL);
    if (has_invalid(sb.data) || !strstr(sb.data, "+eax*")) {
        printf("load idx spill Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    printf("load idx spill tests passed\n");
    return 0;
}


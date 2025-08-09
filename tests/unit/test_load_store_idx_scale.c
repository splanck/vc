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

int main(void) {
    int locs[3] = {0};
    regalloc_t ra = { .loc = locs, .stack_slots = 0 };
    ir_instr_t ins;
    strbuf_t sb;

    ra.loc[1] = 0; /* index register */
    ra.loc[2] = 1; /* dest/value register */
    ins.op = IR_LOAD_IDX;
    ins.dest = 2;
    ins.src1 = 1;
    ins.name = "base";
    ins.type = TYPE_PTR;
    ins.imm = 4;

    /* 32-bit load */
    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 0, ASM_ATT);
    if (!strstr(sb.data, "base(,") || !strstr(sb.data, ",4)")) {
        printf("load idx 32 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 0, ASM_INTEL);
    if (!(strstr(sb.data, "[base+") && strstr(sb.data, "*4]") && !strstr(sb.data, "[[base"))) {
        printf("load idx 32 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 32-bit store */
    ins.op = IR_STORE_IDX;
    ins.src2 = 2;
    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 0, ASM_ATT);
    if (!strstr(sb.data, "base(,") || !strstr(sb.data, ",4)")) {
        printf("store idx 32 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 0, ASM_INTEL);
    if (!(strstr(sb.data, "[base+") && strstr(sb.data, "*4]") && !strstr(sb.data, "[[base"))) {
        printf("store idx 32 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 64-bit load */
    ins.imm = 8;
    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 1, ASM_ATT);
    if (!strstr(sb.data, "base(,") || !strstr(sb.data, ",8)")) {
        printf("load idx 64 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_load_idx(&sb, &ins, &ra, 1, ASM_INTEL);
    if (!(strstr(sb.data, "[base+") && strstr(sb.data, "*8]") && !strstr(sb.data, "[[base"))) {
        printf("load idx 64 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* 64-bit store */
    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 1, ASM_ATT);
    if (!strstr(sb.data, "base(,") || !strstr(sb.data, ",8)")) {
        printf("store idx 64 ATT failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    strbuf_init(&sb);
    emit_store_idx(&sb, &ins, &ra, 1, ASM_INTEL);
    if (!(strstr(sb.data, "[base+") && strstr(sb.data, "*8]") && !strstr(sb.data, "[[base"))) {
        printf("store idx 64 Intel failed: %s\n", sb.data);
        return 1;
    }
    strbuf_free(&sb);

    /* Element size scaling for specific types */
    size_t sizes[] = {1, 2, sizeof(long), sizeof(long long)};
    const char *names[] = {"char", "short", "long", "long long"};
    for (int i = 0; i < 4; ++i) {
        char expect_att[32];
        char expect_intel[32];
        snprintf(expect_att, sizeof(expect_att), ",%zu)", sizes[i]);
        snprintf(expect_intel, sizeof(expect_intel), "*%zu]", sizes[i]);
        ins.imm = (int)sizes[i];

        /* load */
        ins.op = IR_LOAD_IDX;
        strbuf_init(&sb);
        emit_load_idx(&sb, &ins, &ra, 1, ASM_ATT);
        if (!strstr(sb.data, expect_att)) {
            printf("load idx %s failed: %s\n", names[i], sb.data);
            return 1;
        }
        strbuf_free(&sb);

        strbuf_init(&sb);
        emit_load_idx(&sb, &ins, &ra, 1, ASM_INTEL);
        if (!strstr(sb.data, "[base+") ||
            (sizes[i] != 1 && !strstr(sb.data, expect_intel)) ||
            (sizes[i] == 1 && strstr(sb.data, "*"))) {
            printf("load idx %s Intel failed: %s\n", names[i], sb.data);
            return 1;
        }
        strbuf_free(&sb);

        /* store */
        ins.op = IR_STORE_IDX;
        ins.src2 = 2;
        strbuf_init(&sb);
        emit_store_idx(&sb, &ins, &ra, 1, ASM_ATT);
        if (!strstr(sb.data, expect_att)) {
            printf("store idx %s failed: %s\n", names[i], sb.data);
            return 1;
        }
        strbuf_free(&sb);

        strbuf_init(&sb);
        emit_store_idx(&sb, &ins, &ra, 1, ASM_INTEL);
        if (!strstr(sb.data, "[base+") ||
            (sizes[i] != 1 && !strstr(sb.data, expect_intel)) ||
            (sizes[i] == 1 && strstr(sb.data, "*"))) {
            printf("store idx %s Intel failed: %s\n", names[i], sb.data);
            return 1;
        }
        strbuf_free(&sb);
    }

    printf("load/store idx scale tests passed\n");
    return 0;
}

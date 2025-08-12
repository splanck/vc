#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "codegen_loadstore.h"
#include "strbuf.h"
#include "regalloc_x86.h"

/* Minimal fmt_stack implementation to interpret "stack:" operands. */
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

static int check(const char *out, const char *exp, const char *name) {
    if (strcmp(out, exp) != 0) {
        printf("%s unexpected: %s\n", name, out);
        return 1;
    }
    return 0;
}

int main(void) {
    strbuf_t sb;
    int fail = 0;
    int locs[2] = {0, -1};
    regalloc_t ra = { .loc = locs };
    ir_instr_t ins;

    regalloc_set_x86_64(1);

    /* long double load */
    ins.op = IR_LOAD;
    ins.dest = 1;
    ins.name = "stack:16";
    ins.type = TYPE_LDOUBLE;

    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_ATT);
    emit_load(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "    fldt -16(%rbp)\n    fstpt -8(%rbp)\n", "ld load ATT");
    sb.len = 0; if (sb.data) sb.data[0] = '\0';
    regalloc_set_asm_syntax(ASM_INTEL);
    emit_load(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "    fld tword ptr [rbp-16]\n    fstp tword ptr [rbp-8]\n", "ld load Intel");
    strbuf_free(&sb);

    /* long double store */
    ins.op = IR_STORE;
    ins.src1 = 1;
    ins.name = "stack:24";
    strbuf_init(&sb);
    regalloc_set_asm_syntax(ASM_ATT);
    emit_store(&sb, &ins, &ra, 1, ASM_ATT);
    fail |= check(sb.data, "    fldt -8(%rbp)\n    fstpt -24(%rbp)\n", "ld store ATT");
    sb.len = 0; if (sb.data) sb.data[0] = '\0';
    regalloc_set_asm_syntax(ASM_INTEL);
    emit_store(&sb, &ins, &ra, 1, ASM_INTEL);
    fail |= check(sb.data, "    fld tword ptr [rbp-8]\n    fstp tword ptr [rbp-24]\n", "ld store Intel");
    strbuf_free(&sb);

    if (!fail)
        printf("long double load/store tests passed\n");
    return fail;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"
#include "regalloc.h"

/* Simple dynamic string buffer used for assembly output */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf_t;

static void sb_init(strbuf_t *sb)
{
    sb->cap = 128;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (sb->data)
        sb->data[0] = '\0';
}

static void sb_ensure(strbuf_t *sb, size_t extra)
{
    if (sb->len + extra >= sb->cap) {
        size_t new_cap = sb->cap * 2;
        while (sb->len + extra >= new_cap)
            new_cap *= 2;
        char *n = realloc(sb->data, new_cap);
        if (!n)
            return;
        sb->data = n;
        sb->cap = new_cap;
    }
}

static void sb_append(strbuf_t *sb, const char *text)
{
    size_t l = strlen(text);
    sb_ensure(sb, l + 1);
    if (!sb->data)
        return;
    memcpy(sb->data + sb->len, text, l + 1);
    sb->len += l;
}

static void sb_appendf(strbuf_t *sb, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((size_t)n >= sizeof(buf)) {
        char *tmp = malloc((size_t)n + 1);
        if (!tmp)
            return;
        va_start(ap, fmt);
        vsnprintf(tmp, (size_t)n + 1, fmt, ap);
        va_end(ap);
        sb_append(sb, tmp);
        free(tmp);
    } else {
        sb_append(sb, buf);
    }
}


static const char *loc_str(char buf[32], regalloc_t *ra, int id)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return regalloc_reg_name(loc);
    snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    return buf;
}

static void emit_instr(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra)
{
    char buf1[32];
    char buf2[32];
    switch (ins->op) {
    case IR_CONST:
        sb_appendf(sb, "    movl $%d, %s\n", ins->imm,
                   loc_str(buf1, ra, ins->dest));
        break;
    case IR_LOAD:
        sb_appendf(sb, "    movl %s, %s\n", ins->name,
                   loc_str(buf1, ra, ins->dest));
        break;
    case IR_STORE:
        sb_appendf(sb, "    movl %s, %s\n", loc_str(buf1, ra, ins->src1),
                   ins->name);
        break;
    case IR_ADDR:
        sb_appendf(sb, "    movl $%s, %s\n", ins->name,
                   loc_str(buf1, ra, ins->dest));
        break;
    case IR_LOAD_PTR:
        sb_appendf(sb, "    movl (%s), %s\n",
                   loc_str(buf1, ra, ins->src1),
                   loc_str(buf2, ra, ins->dest));
        break;
    case IR_STORE_PTR:
        sb_appendf(sb, "    movl %s, (%s)\n",
                   loc_str(buf1, ra, ins->src2),
                   loc_str(buf2, ra, ins->src1));
        break;
    case IR_ADD:
        sb_appendf(sb, "    movl %s, %s\n",
                   loc_str(buf1, ra, ins->src1),
                   loc_str(buf2, ra, ins->dest));
        sb_appendf(sb, "    addl %s, %s\n",
                   loc_str(buf1, ra, ins->src2),
                   loc_str(buf2, ra, ins->dest));
        break;
    case IR_SUB:
        sb_appendf(sb, "    movl %s, %s\n",
                   loc_str(buf1, ra, ins->src1),
                   loc_str(buf2, ra, ins->dest));
        sb_appendf(sb, "    subl %s, %s\n",
                   loc_str(buf1, ra, ins->src2),
                   loc_str(buf2, ra, ins->dest));
        break;
    case IR_MUL:
        sb_appendf(sb, "    movl %s, %s\n",
                   loc_str(buf1, ra, ins->src1),
                   loc_str(buf2, ra, ins->dest));
        sb_appendf(sb, "    imull %s, %s\n",
                   loc_str(buf1, ra, ins->src2),
                   loc_str(buf2, ra, ins->dest));
        break;
    case IR_DIV:
        sb_appendf(sb, "    movl %s, %s\n",
                   loc_str(buf1, ra, ins->src1), "%eax");
        sb_append(sb, "    cltd\n");
        sb_appendf(sb, "    idivl %s\n", loc_str(buf1, ra, ins->src2));
        if (ra && ra->loc[ins->dest] >= 0 &&
            strcmp(regalloc_reg_name(ra->loc[ins->dest]), "%eax") != 0)
            sb_appendf(sb, "    movl %s, %s\n", "%eax",
                       loc_str(buf2, ra, ins->dest));
        break;
    case IR_GLOB_STRING:
        sb_appendf(sb, "%s:\n", ins->name);
        sb_appendf(sb, "    .asciz \"%s\"\n", ins->data);
        sb_appendf(sb, "    movl $%s, %s\n", ins->name,
                   loc_str(buf1, ra, ins->dest));
        break;
    case IR_RETURN:
        sb_appendf(sb, "    movl %s, %s\n",
                   loc_str(buf1, ra, ins->src1), "%eax");
        sb_append(sb, "    ret\n");
        break;
    case IR_CALL:
        sb_appendf(sb, "    call %s\n", ins->name);
        sb_appendf(sb, "    movl %s, %s\n", "%eax",
                   loc_str(buf1, ra, ins->dest));
        break;
    case IR_FUNC_BEGIN:
        sb_appendf(sb, "%s:\n", ins->name);
        sb_append(sb, "    pushl %ebp\n");
        sb_append(sb, "    movl %esp, %ebp\n");
        break;
    case IR_FUNC_END:
        /* nothing for now */
        break;
    case IR_BR:
        sb_appendf(sb, "    jmp %s\n", ins->name);
        break;
    case IR_BCOND:
        sb_appendf(sb, "    cmpl $0, %s\n", loc_str(buf1, ra, ins->src1));
        sb_appendf(sb, "    je %s\n", ins->name);
        break;
    case IR_LABEL:
        sb_appendf(sb, "%s:\n", ins->name);
        break;
    }
}

char *codegen_ir_to_string(ir_builder_t *ir)
{
    if (!ir)
        return NULL;
    regalloc_t ra;
    regalloc_run(ir, &ra);

    strbuf_t sb;
    sb_init(&sb);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next)
        emit_instr(&sb, ins, &ra);

    regalloc_free(&ra);
    return sb.data; /* caller takes ownership */
}

void codegen_emit_x86(FILE *out, ir_builder_t *ir)
{
    if (!out)
        return;
    char *text = codegen_ir_to_string(ir);
    if (text) {
        fputs(text, out);
        free(text);
    }
}


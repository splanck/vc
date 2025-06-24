#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "codegen.h"

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

static const char *reg_for(int id)
{
    static const char *regs[] = {"%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"};
    int idx = (id - 1) % 6;
    if (idx < 0)
        idx = 0;
    return regs[idx];
}

static void emit_instr(strbuf_t *sb, ir_instr_t *ins)
{
    switch (ins->op) {
    case IR_CONST:
        sb_appendf(sb, "    movl $%d, %s\n", ins->imm, reg_for(ins->dest));
        break;
    case IR_LOAD:
        sb_appendf(sb, "    movl %s, %s\n", ins->name, reg_for(ins->dest));
        break;
    case IR_STORE:
        sb_appendf(sb, "    movl %s, %s\n", reg_for(ins->src1), ins->name);
        break;
    case IR_ADD:
        sb_appendf(sb, "    movl %s, %s\n", reg_for(ins->src1), reg_for(ins->dest));
        sb_appendf(sb, "    addl %s, %s\n", reg_for(ins->src2), reg_for(ins->dest));
        break;
    case IR_SUB:
        sb_appendf(sb, "    movl %s, %s\n", reg_for(ins->src1), reg_for(ins->dest));
        sb_appendf(sb, "    subl %s, %s\n", reg_for(ins->src2), reg_for(ins->dest));
        break;
    case IR_MUL:
        sb_appendf(sb, "    movl %s, %s\n", reg_for(ins->src1), reg_for(ins->dest));
        sb_appendf(sb, "    imull %s, %s\n", reg_for(ins->src2), reg_for(ins->dest));
        break;
    case IR_DIV:
        sb_appendf(sb, "    movl %s, %s\n", reg_for(ins->src1), "%eax");
        sb_append(sb, "    cltd\n");
        sb_appendf(sb, "    idivl %s\n", reg_for(ins->src2));
        if (strcmp(reg_for(ins->dest), "%eax") != 0)
            sb_appendf(sb, "    movl %s, %s\n", "%eax", reg_for(ins->dest));
        break;
    case IR_RETURN:
        sb_appendf(sb, "    movl %s, %s\n", reg_for(ins->src1), "%eax");
        sb_append(sb, "    ret\n");
        break;
    case IR_CALL:
        sb_appendf(sb, "    call %s\n", ins->name);
        sb_appendf(sb, "    movl %s, %s\n", "%eax", reg_for(ins->dest));
        break;
    case IR_FUNC_BEGIN:
        sb_appendf(sb, "%s:\n", ins->name);
        sb_append(sb, "    pushl %ebp\n");
        sb_append(sb, "    movl %esp, %ebp\n");
        break;
    case IR_FUNC_END:
        /* nothing for now */
        break;
    }
}

char *codegen_ir_to_string(ir_builder_t *ir)
{
    if (!ir)
        return NULL;
    strbuf_t sb;
    sb_init(&sb);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next)
        emit_instr(&sb, ins);
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


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


static const char *loc_str(char buf[32], regalloc_t *ra, int id, int x64)
{
    if (!ra || id <= 0)
        return "";
    int loc = ra->loc[id];
    if (loc >= 0)
        return regalloc_reg_name(loc);
    if (x64)
        snprintf(buf, 32, "-%d(%%rbp)", -loc * 8);
    else
        snprintf(buf, 32, "-%d(%%ebp)", -loc * 4);
    return buf;
}

static void emit_instr(strbuf_t *sb, ir_instr_t *ins, regalloc_t *ra, int x64)
{
    char buf1[32];
    char buf2[32];
    const char *sfx = x64 ? "q" : "l";
    const char *ax = x64 ? "%rax" : "%eax";
    const char *bp = x64 ? "%rbp" : "%ebp";
    const char *sp = x64 ? "%rsp" : "%esp";
    switch (ins->op) {
    case IR_CONST:
        sb_appendf(sb, "    mov%s $%d, %s\n", sfx, ins->imm,
                   loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_LOAD:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx, ins->name,
                   loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_STORE:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64),
                   ins->name);
        break;
    case IR_LOAD_PARAM: {
        int off = 8 + ins->imm * (x64 ? 8 : 4);
        sb_appendf(sb, "    mov%s %d(%s), %s\n", sfx, off, bp,
                   loc_str(buf1, ra, ins->dest, x64));
        break;
    }
    case IR_STORE_PARAM: {
        int off = 8 + ins->imm * (x64 ? 8 : 4);
        sb_appendf(sb, "    mov%s %s, %d(%s)\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64), off, bp);
        break;
    }
    case IR_ADDR:
        sb_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name,
                   loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_LOAD_PTR:
        sb_appendf(sb, "    mov%s (%s), %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_STORE_PTR:
        sb_appendf(sb, "    mov%s %s, (%s)\n", sfx,
                   loc_str(buf1, ra, ins->src2, x64),
                   loc_str(buf2, ra, ins->src1, x64));
        break;
    case IR_ADD:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        sb_appendf(sb, "    add%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src2, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_SUB:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        sb_appendf(sb, "    sub%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src2, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_MUL:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        sb_appendf(sb, "    imul%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src2, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_DIV:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64), ax);
        sb_appendf(sb, "    %s\n", x64 ? "cqto" : "cltd");
        sb_appendf(sb, "    idiv%s %s\n", sfx,
                   loc_str(buf1, ra, ins->src2, x64));
        if (ra && ra->loc[ins->dest] >= 0 &&
            strcmp(regalloc_reg_name(ra->loc[ins->dest]), ax) != 0)
            sb_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                       loc_str(buf2, ra, ins->dest, x64));
        break;
    case IR_CMPEQ: case IR_CMPNE: case IR_CMPLT: case IR_CMPGT:
    case IR_CMPLE: case IR_CMPGE: {
        const char *cc = "";
        switch (ins->op) {
        case IR_CMPEQ: cc = "e"; break;
        case IR_CMPNE: cc = "ne"; break;
        case IR_CMPLT: cc = "l"; break;
        case IR_CMPGT: cc = "g"; break;
        case IR_CMPLE: cc = "le"; break;
        case IR_CMPGE: cc = "ge"; break;
        default: break;
        }
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        sb_appendf(sb, "    cmp%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src2, x64),
                   loc_str(buf2, ra, ins->dest, x64));
        sb_appendf(sb, "    set%s %s\n", cc, "%al");
        sb_appendf(sb, "    %s %s, %s\n", x64 ? "movzbq" : "movzbl",
                   "%al", loc_str(buf2, ra, ins->dest, x64));
        break;
    }
    case IR_GLOB_STRING:
        sb_appendf(sb, "    mov%s $%s, %s\n", sfx, ins->name,
                   loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_GLOB_VAR:
        /* globals handled separately in data section */
        break;
    case IR_RETURN:
        sb_appendf(sb, "    mov%s %s, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64), ax);
        sb_append(sb, "    ret\n");
        break;
    case IR_CALL:
        sb_appendf(sb, "    call %s\n", ins->name);
        sb_appendf(sb, "    mov%s %s, %s\n", sfx, ax,
                   loc_str(buf1, ra, ins->dest, x64));
        break;
    case IR_FUNC_BEGIN:
        sb_appendf(sb, "%s:\n", ins->name);
        sb_appendf(sb, "    push%s %s\n", sfx, bp);
        sb_appendf(sb, "    mov%s %s, %s\n", sfx, sp, bp);
        break;
    case IR_FUNC_END:
        /* nothing for now */
        break;
    case IR_BR:
        sb_appendf(sb, "    jmp %s\n", ins->name);
        break;
    case IR_BCOND:
        sb_appendf(sb, "    cmp%s $0, %s\n", sfx,
                   loc_str(buf1, ra, ins->src1, x64));
        sb_appendf(sb, "    je %s\n", ins->name);
        break;
    case IR_LABEL:
        sb_appendf(sb, "%s:\n", ins->name);
        break;
    }
}

char *codegen_ir_to_string(ir_builder_t *ir, int x64)
{
    if (!ir)
        return NULL;
    regalloc_t ra;
    regalloc_set_x86_64(x64);
    regalloc_run(ir, &ra);

    strbuf_t sb;
    sb_init(&sb);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next)
        emit_instr(&sb, ins, &ra, x64);

    regalloc_free(&ra);
    return sb.data; /* caller takes ownership */
}

void codegen_emit_x86(FILE *out, ir_builder_t *ir, int x64)
{
    if (!out)
        return;
    const char *size_directive = x64 ? ".quad" : ".long";
    int has_data = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_GLOB_VAR || ins->op == IR_GLOB_STRING) {
            if (!has_data) {
                fputs(".data\n", out);
                has_data = 1;
            }
            fprintf(out, "%s:\n", ins->name);
            if (ins->op == IR_GLOB_VAR)
                fprintf(out, "    %s 0\n", size_directive);
            else
                fprintf(out, "    .asciz \"%s\"\n", ins->data);
        }
    }
    if (has_data)
        fputs(".text\n", out);

    char *text = codegen_ir_to_string(ir, x64);
    if (text) {
        fputs(text, out);
        free(text);
    }
}


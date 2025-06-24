#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "ir.h"

/* Simple dynamic string buffer used for IR dumps */
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

static char *dup_string(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len + 1);
    return out;
}

void ir_builder_init(ir_builder_t *b)
{
    b->head = b->tail = NULL;
    b->next_value_id = 1;
}

void ir_builder_free(ir_builder_t *b)
{
    ir_instr_t *ins = b->head;
    while (ins) {
        ir_instr_t *next = ins->next;
        free(ins->name);
        free(ins->data);
        free(ins);
        ins = next;
    }
    b->head = b->tail = NULL;
    b->next_value_id = 0;
}

static ir_instr_t *append_instr(ir_builder_t *b)
{
    ir_instr_t *ins = calloc(1, sizeof(*ins));
    if (!ins)
        return NULL;
    ins->dest = -1;
    ins->name = NULL;
    ins->data = NULL;
    if (!b->head)
        b->head = ins;
    else
        b->tail->next = ins;
    b->tail = ins;
    return ins;
}

ir_value_t ir_build_const(ir_builder_t *b, int value)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CONST;
    ins->dest = b->next_value_id++;
    ins->imm = value;
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_string(ir_builder_t *b, const char *str)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_GLOB_STRING;
    ins->dest = b->next_value_id++;
    char label[32];
    snprintf(label, sizeof(label), "Lstr%d", ins->dest);
    ins->name = dup_string(label);
    ins->data = dup_string(str ? str : "");
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD;
    ins->dest = b->next_value_id++;
    ins->name = dup_string(name ? name : "");
    return (ir_value_t){ins->dest};
}

void ir_build_store(ir_builder_t *b, const char *name, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE;
    ins->src1 = val.id;
    ins->name = dup_string(name ? name : "");
}

ir_value_t ir_build_load_param(ir_builder_t *b, int index)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PARAM;
    ins->dest = b->next_value_id++;
    ins->imm = index;
    return (ir_value_t){ins->dest};
}

void ir_build_store_param(ir_builder_t *b, int index, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PARAM;
    ins->imm = index;
    ins->src1 = val.id;
}

ir_value_t ir_build_addr(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_ADDR;
    ins->dest = b->next_value_id++;
    ins->name = dup_string(name ? name : "");
    return (ir_value_t){ins->dest};
}

ir_value_t ir_build_load_ptr(ir_builder_t *b, ir_value_t addr)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_PTR;
    ins->dest = b->next_value_id++;
    ins->src1 = addr.id;
    return (ir_value_t){ins->dest};
}

void ir_build_store_ptr(ir_builder_t *b, ir_value_t addr, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_PTR;
    ins->src1 = addr.id;
    ins->src2 = val.id;
}

ir_value_t ir_build_load_idx(ir_builder_t *b, const char *name, ir_value_t idx)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_LOAD_IDX;
    ins->dest = b->next_value_id++;
    ins->src1 = idx.id;
    ins->name = dup_string(name ? name : "");
    return (ir_value_t){ins->dest};
}

void ir_build_store_idx(ir_builder_t *b, const char *name, ir_value_t idx,
                        ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_STORE_IDX;
    ins->src1 = idx.id;
    ins->src2 = val.id;
    ins->name = dup_string(name ? name : "");
}

ir_value_t ir_build_binop(ir_builder_t *b, ir_op_t op, ir_value_t left, ir_value_t right)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = op;
    ins->dest = b->next_value_id++;
    ins->src1 = left.id;
    ins->src2 = right.id;
    return (ir_value_t){ins->dest};
}

void ir_build_arg(ir_builder_t *b, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_ARG;
    ins->src1 = val.id;
}

void ir_build_return(ir_builder_t *b, ir_value_t val)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_RETURN;
    ins->src1 = val.id;
}

ir_value_t ir_build_call(ir_builder_t *b, const char *name, size_t arg_count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return (ir_value_t){0};
    ins->op = IR_CALL;
    ins->dest = b->next_value_id++;
    ins->name = dup_string(name ? name : "");
    ins->imm = (int)arg_count;
    return (ir_value_t){ins->dest};
}

void ir_build_func_begin(ir_builder_t *b, const char *name)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_FUNC_BEGIN;
    ins->name = dup_string(name ? name : "");
}

void ir_build_func_end(ir_builder_t *b)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_FUNC_END;
}

void ir_build_br(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BR;
    ins->name = dup_string(label ? label : "");
}

void ir_build_bcond(ir_builder_t *b, ir_value_t cond, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_BCOND;
    ins->src1 = cond.id;
    ins->name = dup_string(label ? label : "");
}

void ir_build_label(ir_builder_t *b, const char *label)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_LABEL;
    ins->name = dup_string(label ? label : "");
}

void ir_build_glob_var(ir_builder_t *b, const char *name, int value)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_VAR;
    ins->name = dup_string(name ? name : "");
    ins->imm = value;
}

void ir_build_glob_array(ir_builder_t *b, const char *name,
                         const int *values, size_t count)
{
    ir_instr_t *ins = append_instr(b);
    if (!ins)
        return;
    ins->op = IR_GLOB_ARRAY;
    ins->name = dup_string(name ? name : "");
    ins->imm = (int)count;
    if (count) {
        int *vals = malloc(count * sizeof(int));
        if (!vals)
            return;
        for (size_t i = 0; i < count; i++)
            vals[i] = values[i];
        ins->data = (char *)vals;
    }
}

static const char *op_name(ir_op_t op)
{
    switch (op) {
    case IR_CONST: return "IR_CONST";
    case IR_ADD: return "IR_ADD";
    case IR_SUB: return "IR_SUB";
    case IR_MUL: return "IR_MUL";
    case IR_DIV: return "IR_DIV";
    case IR_PTR_ADD: return "IR_PTR_ADD";
    case IR_PTR_DIFF: return "IR_PTR_DIFF";
    case IR_CMPEQ: return "IR_CMPEQ";
    case IR_CMPNE: return "IR_CMPNE";
    case IR_CMPLT: return "IR_CMPLT";
    case IR_CMPGT: return "IR_CMPGT";
    case IR_CMPLE: return "IR_CMPLE";
    case IR_CMPGE: return "IR_CMPGE";
    case IR_GLOB_STRING: return "IR_GLOB_STRING";
    case IR_GLOB_VAR: return "IR_GLOB_VAR";
    case IR_GLOB_ARRAY: return "IR_GLOB_ARRAY";
    case IR_LOAD: return "IR_LOAD";
    case IR_STORE: return "IR_STORE";
    case IR_LOAD_PARAM: return "IR_LOAD_PARAM";
    case IR_STORE_PARAM: return "IR_STORE_PARAM";
    case IR_ADDR: return "IR_ADDR";
    case IR_LOAD_PTR: return "IR_LOAD_PTR";
    case IR_STORE_PTR: return "IR_STORE_PTR";
    case IR_LOAD_IDX: return "IR_LOAD_IDX";
    case IR_STORE_IDX: return "IR_STORE_IDX";
    case IR_ARG: return "IR_ARG";
    case IR_RETURN: return "IR_RETURN";
    case IR_CALL: return "IR_CALL";
    case IR_FUNC_BEGIN: return "IR_FUNC_BEGIN";
    case IR_FUNC_END: return "IR_FUNC_END";
    case IR_BR: return "IR_BR";
    case IR_BCOND: return "IR_BCOND";
    case IR_LABEL: return "IR_LABEL";
    }
    return "";
}

char *ir_to_string(ir_builder_t *ir)
{
    if (!ir)
        return NULL;

    strbuf_t sb;
    sb_init(&sb);
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op == IR_GLOB_ARRAY) {
            sb_appendf(&sb, "%s name=%s count=%d\n", op_name(ins->op),
                       ins->name ? ins->name : "", ins->imm);
        } else {
            sb_appendf(&sb, "%s dest=%d src1=%d src2=%d imm=%d name=%s data=%s\n",
                       op_name(ins->op), ins->dest, ins->src1, ins->src2,
                       ins->imm, ins->name ? ins->name : "",
                       ins->data ? ins->data : "");
        }
    }
    return sb.data; /* caller frees */
}


/*
 * Simple function inlining pass.
 *
 * Replaces calls to small functions consisting of two parameter loads,
 * a single binary operation and a return with the operation itself.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "opt.h"

typedef struct {
    const char *name;
    ir_op_t op;
} inline_func_t;

/*
 * Check if a function definition in the given source file has the
 * 'inline' keyword appearing before its return type.  This is a very
 * small parser that scans for a line defining the function and then
 * tokenizes the portion preceding the function name.
 */
static int is_inline_def(const char *file, const char *name)
{
    FILE *f = fopen(file, "r");
    if (!f)
        return -1;

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int in_comment = 0;
    int result = 0;

    while ((nread = getline(&line, &len, f)) != -1) {
        char *s = line;

        /* handle multi-line comments */
        if (in_comment) {
            char *end = strstr(s, "*/");
            if (!end)
                continue;
            s = end + 2;
            in_comment = 0;
        }
        while (!in_comment) {
            char *start = strstr(s, "/*");
            if (!start)
                break;
            char *end = strstr(start + 2, "*/");
            if (end) {
                memmove(start, end + 2, strlen(end + 2) + 1);
            } else {
                *start = '\0';
                in_comment = 1;
                break;
            }
        }
        if (in_comment)
            continue;

        /* strip line comments */
        char *slashslash = strstr(s, "//");
        if (slashslash)
            *slashslash = '\0';

        /* search for "name(" followed by ')' and '{' on the same line */
        char *pos = strstr(s, name);
        if (!pos)
            continue;
        if (pos > s && (isalnum((unsigned char)pos[-1]) || pos[-1] == '_'))
            continue;
        char *after = pos + strlen(name);
        while (isspace((unsigned char)*after))
            after++;
        if (*after != '(')
            continue;
        char *close = strchr(after, ')');
        if (!close)
            continue;
        char *brace = strchr(close, '{');
        if (!brace)
            continue;

        /* tokenize the prefix before the function name */
        char prefix[512];
        size_t pre_len = (size_t)(pos - s);
        if (pre_len >= sizeof(prefix))
            pre_len = sizeof(prefix) - 1;
        memcpy(prefix, s, pre_len);
        prefix[pre_len] = '\0';

        char *tokens[64];
        size_t ntok = 0;
        char *tok = strtok(prefix, " \t\r\n*");
        while (tok && ntok < 64) {
            tokens[ntok++] = tok;
            tok = strtok(NULL, " \t\r\n*");
        }
        for (size_t i = 0; i + 1 < ntok; i++) {
            if (strcmp(tokens[i], "inline") == 0) {
                result = 1;
                break;
            }
        }
        break; /* processed definition line */
    }

    free(line);
    fclose(f);
    return result;
}

static int is_simple_op(ir_op_t op)
{
    switch (op) {
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
    case IR_MOD: case IR_FADD: case IR_FSUB: case IR_FMUL:
    case IR_FDIV: case IR_LFADD: case IR_LFSUB: case IR_LFMUL:
    case IR_LFDIV:
        return 1;
    default:
        return 0;
    }
}

/* Identify eligible functions and store their name/op pairs */
static int collect_funcs(ir_builder_t *ir, inline_func_t **out, size_t *count)
{
    *out = NULL;
    *count = 0;
    size_t cap = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op != IR_FUNC_BEGIN)
            continue;
        ir_instr_t *p1 = ins->next;
        ir_instr_t *p2 = p1 ? p1->next : NULL;
        ir_instr_t *op = p2 ? p2->next : NULL;
        ir_instr_t *ret = op ? op->next : NULL;
        ir_instr_t *end = ret ? ret->next : NULL;
        if (!p1 || !p2 || !op || !ret || !end)
            continue;
        if (p1->op != IR_LOAD_PARAM || p1->imm != 0)
            continue;
        if (p2->op != IR_LOAD_PARAM || p2->imm != 1)
            continue;
        if (!is_simple_op(op->op))
            continue;
        if (ret->op != IR_RETURN || ret->src1 != op->dest)
            continue;
        if (end->op != IR_FUNC_END)
            continue;

        /* Check that the defining line marks the function as inline */
        int is_inline = 0;
        const char *src_file = p1 ? p1->file : NULL;
        if (src_file) {
            int r = is_inline_def(src_file, ins->name);
            if (r > 0) {
                is_inline = 1;
            } else if (r < 0) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "could not open %s for inline check; "
                         "treating %s as non-inline",
                         src_file, ins->name);
                opt_error(msg);
            }
        }
        if (!is_inline) {
            continue;
        }
        if (*count == cap) {
            cap = cap ? cap * 2 : 4;
            inline_func_t *tmp = realloc(*out, cap * sizeof(**out));
            if (!tmp) {
                opt_error("out of memory");
                free(*out);
                return 0;
            }
            *out = tmp;
        }
        (*out)[(*count)++] = (inline_func_t){ins->name, op->op};
    }
    return 1;
}

/* Remove instruction at list[index] from both array and linked list */
static void remove_instr(ir_builder_t *ir, ir_instr_t **list, int *count, int index)
{
    ir_instr_t *prev = (index > 0) ? list[index - 1] : NULL;
    ir_instr_t *ins = list[index];
    ir_instr_t *next = ins->next;
    if (prev)
        prev->next = next;
    else
        ir->head = next;
    if (ins == ir->tail)
        ir->tail = prev;
    free(ins->name);
    free(ins->data);
    free(ins);
    for (int i = index; i < *count - 1; i++)
        list[i] = list[i + 1];
    (*count)--;
}

/* Recompute tail pointer after all changes */
static void recompute_tail(ir_builder_t *ir)
{
    ir_instr_t *t = ir->head;
    if (!t) { ir->tail = NULL; return; }
    while (t->next) t = t->next;
    ir->tail = t;
}

void inline_small_funcs(ir_builder_t *ir)
{
    if (!ir)
        return;

    inline_func_t *funcs = NULL;
    size_t func_count = 0;
    if (!collect_funcs(ir, &funcs, &func_count))
        return;

    int count = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        count++;
    if (count == 0) { free(funcs); return; }
    ir_instr_t **list = malloc((size_t)count * sizeof(*list));
    if (!list) {
        opt_error("out of memory");
        free(funcs);
        return;
    }
    int idx = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        list[idx++] = it;

    for (int i = 0; i < count; i++) {
        ir_instr_t *ins = list[i];
        if (ins->op != IR_CALL)
            continue;
        inline_func_t *fn = NULL;
        for (size_t j = 0; j < func_count; j++) {
            if (strcmp(funcs[j].name, ins->name) == 0) {
                fn = &funcs[j];
                break;
            }
        }
        if (!fn || ins->imm != 2 || i < 2)
            continue;
        if (list[i-1]->op != IR_ARG || list[i-2]->op != IR_ARG)
            continue;
        int arg0 = list[i-2]->src1;
        int arg1 = list[i-1]->src1;
        remove_instr(ir, list, &count, i-1); /* remove second arg */
        remove_instr(ir, list, &count, i-2); /* remove first arg */
        i -= 2; /* adjust index to new position of call */
        ins->op = fn->op;
        ins->src1 = arg0;
        ins->src2 = arg1;
        ins->imm = 0;
        free(ins->name);
        ins->name = NULL;
    }

    recompute_tail(ir);
    free(list);
    free(funcs);
}


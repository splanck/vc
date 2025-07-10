/*
 * Simple function inlining pass.
 *
 * Replaces calls to small functions with short bodies.  Functions
 * consisting of two parameter loads and a single arithmetic operation
 * are currently inlined directly into the call site.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include "opt.h"
#include "error.h"
#include "util.h"

typedef struct {
    const char *name;
    ir_instr_t *body;
    size_t count;      /* number of instructions in body */
    int param_count;   /* number of parameters */
} inline_func_t;

/*
 * Check if a function definition in the given source file has the
 * 'inline' keyword appearing before its return type.  This is a very
 * small parser that scans for a line defining the function and then
 * tokenizes the portion preceding the function name.
 */
static void strip_comments(char *s, int *in_comment)
{
    if (*in_comment) {
        char *end = strstr(s, "*/");
        if (!end) {
            *s = '\0';
            return;
        }
        memmove(s, end + 2, strlen(end + 2) + 1);
        *in_comment = 0;
    }
    while (!*in_comment) {
        char *start = strstr(s, "/*");
        if (!start)
            break;
        char *end = strstr(start + 2, "*/");
        if (end) {
            memmove(start, end + 2, strlen(end + 2) + 1);
        } else {
            *start = '\0';
            *in_comment = 1;
            break;
        }
    }
    char *slash = strstr(s, "//");
    if (slash)
        *slash = '\0';
}

static int tokens_have_inline(const char *prefix)
{
    char *buf = vc_strdup(prefix);
    if (!buf)
        return -1;
    int result = 0;
    char *tok = strtok(buf, " \t\r\n*");
    while (tok) {
        if (strcmp(tok, "inline") == 0) {
            result = 1;
            break;
        }
        tok = strtok(NULL, " \t\r\n*");
    }
    free(buf);
    return result;
}

static int parse_inline_hint(const char *file, const char *name)
{
    char *line = NULL;
    FILE *f = fopen(file, "r");
    if (!f) {
        /* errno is preserved for callers */
        return -1;
    }

    size_t len = 0;
    ssize_t nread;
    int in_comment = 0;
    int result = 0;

    while ((nread = getline(&line, &len, f)) != -1) {
        strip_comments(line, &in_comment);
        if (in_comment)
            continue;
        char *pos = strstr(line, name);
        if (!pos)
            continue;
        if (pos > line && (isalnum((unsigned char)pos[-1]) || pos[-1] == '_'))
            continue;
        char *after = pos + strlen(name);
        while (isspace((unsigned char)*after))
            after++;
        if (*after != '(')
            continue;
        char *close = strchr(after, ')');
        if (!close || !strchr(close, '{'))
            continue;
        size_t pre_len = (size_t)(pos - line);
        char *prefix = vc_strndup(line, pre_len);
        if (!prefix) {
            free(line);
            fclose(f);
            errno = ENOMEM;
            return -1;
        }
        int r = tokens_have_inline(prefix);
        free(prefix);
        if (r < 0) {
            free(line);
            fclose(f);
            errno = ENOMEM;
            return -1;
        }
        result = r;
        break;
    }

    if (ferror(f)) {
        free(line);
        fclose(f);
        return -1;
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

/*
 * Clone the instructions inside a function body.  `begin` must point
 * to an IR_FUNC_BEGIN instruction.  On success, a newly allocated array
 * of instructions is stored in `*out` and the number of entries in
 * `*count`.  Only instructions recognised by `is_simple_op`,
 * IR_LOAD_PARAM, IR_CONST, IR_RETURN and IR_RETURN_AGG are permitted.  The total number
 * of arithmetic operations must not exceed four.
 */
static int clone_inline_body(ir_instr_t *begin, ir_instr_t **out, size_t *count)
{
    *out = NULL;
    *count = 0;
    if (!begin || begin->op != IR_FUNC_BEGIN)
        return 0;

    size_t n = 0;
    int arith = 0;
    for (ir_instr_t *it = begin->next; it && it->op != IR_FUNC_END; it = it->next) {
        if (is_simple_op(it->op)) {
            if (++arith > 4)
                return 0;
        } else if (it->op != IR_LOAD_PARAM && it->op != IR_CONST &&
                   it->op != IR_RETURN && it->op != IR_RETURN_AGG) {
            return 0;
        }
        n++;
    }

    ir_instr_t *end = begin->next;
    while (end && end->op != IR_FUNC_END)
        end = end->next;
    if (!end)
        return 0;

    if (n == 0)
        return 0;

    ir_instr_t *buf = malloc(n * sizeof(*buf));
    if (!buf) {
        opt_error("out of memory");
        return 0;
    }

    ir_instr_t *src = begin->next;
    for (size_t i = 0; i < n; i++, src = src->next) {
        buf[i] = *src;
        buf[i].next = NULL;
    }

    *out = buf;
    *count = n;
    return 1;
}

/* Identify eligible functions and store their name/op pairs */
/*
 * Identify inline functions with bodies accepted by clone_inline_body.
 * For each candidate, store a copy of the instructions (excluding
 * FUNC_BEGIN/END) and the number of parameters used.
 */
static int collect_funcs(ir_builder_t *ir, inline_func_t **out, size_t *count)
{
    *out = NULL;
    *count = 0;
    size_t cap = 0;
    for (ir_instr_t *ins = ir->head; ins; ins = ins->next) {
        if (ins->op != IR_FUNC_BEGIN)
            continue;
        ir_instr_t *body = NULL;
        size_t body_count = 0;
        if (!clone_inline_body(ins, &body, &body_count))
            continue;

        int param_count = 0;
        for (size_t i = 0; i < body_count; i++)
            if (body[i].op == IR_LOAD_PARAM && body[i].imm + 1 > param_count)
                param_count = body[i].imm + 1;

        /* Check that the defining line marks the function as inline */
        int is_inline = 0;
        const char *src_file = ins->next ? ins->next->file : NULL;
        if (src_file) {
            int r = parse_inline_hint(src_file, ins->name);
            if (r > 0) {
                is_inline = 1;
            } else if (r < 0) {
                if (errno != ENAMETOOLONG) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "could not open %s for inline check: %s",
                             src_file, strerror(errno));
                    opt_error(msg);
                }
                free(*out);
                *out = NULL;
                free(body);
                return 0;
            }
        }
        if (!is_inline) {
            free(body);
            continue;
        }
        if (*count == cap) {
            size_t max_cap = SIZE_MAX / sizeof(**out);
            size_t new_cap;
            if (cap) {
                if (cap > max_cap / 2) {
                    opt_error("too many inline functions");
                    free(*out);
                    *out = NULL;
                    free(body);
                    return 0;
                }
                new_cap = cap * 2;
            } else {
                new_cap = 4;
            }
            if (new_cap > max_cap)
                new_cap = max_cap;
            inline_func_t *tmp = realloc(*out, new_cap * sizeof(**out));
            if (!tmp) {
                opt_error("out of memory");
                free(body);
                return 0;
            }
            *out = tmp;
            cap = new_cap;
        }
        (*out)[(*count)++] =
            (inline_func_t){ins->name, body, body_count, param_count};
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

typedef struct {
    int old_id;
    int new_id;
    ir_instr_t *ins;
} map_entry_t;

static int gather_call_args(ir_instr_t **list, int idx, int argc, int *args)
{
    for (int a = 0; a < argc; a++) {
        ir_instr_t *arg = list[idx - argc + a];
        if (!arg || arg->op != IR_ARG)
            return 0;
        args[a] = arg->src1;
    }
    return 1;
}

static void replace_value_uses(ir_instr_t **list, int start, int count,
                              int old, int new)
{
    for (int u = start; u < count; u++) {
        if (list[u]->src1 == old)
            list[u]->src1 = new;
        if (list[u]->src2 == old)
            list[u]->src2 = new;
    }
}

static int map_lookup(map_entry_t *map, size_t count, int old)
{
    for (size_t i = 0; i < count; i++)
        if (map[i].old_id == old)
            return map[i].new_id;
    return old;
}

static int insert_inline_body(ir_builder_t *ir, ir_instr_t *call,
                              inline_func_t *fn, int argc, int *args,
                              int *ret_val)
{
    map_entry_t map[32];
    size_t mcount = 0;
    ir_instr_t *pos = call;
    *ret_val = call->dest;

    for (size_t k = 0; k < fn->count; k++) {
        ir_instr_t *orig = &fn->body[k];
        if (orig->op == IR_LOAD_PARAM) {
            if (orig->imm >= argc)
                return 0;
            map[mcount++] = (map_entry_t){orig->dest, args[orig->imm], NULL};
            continue;
        }
        if (orig->op == IR_RETURN || orig->op == IR_RETURN_AGG) {
            *ret_val = map_lookup(map, mcount, orig->src1);
            for (size_t m = 0; m < mcount; m++) {
                if (map[m].old_id == orig->src1 && map[m].ins) {
                    map[m].ins->dest = call->dest;
                    map[m].new_id = call->dest;
                    *ret_val = call->dest;
                    break;
                }
            }
            continue;
        }

        ir_instr_t *ni = ir_insert_after(ir, pos);
        if (!ni)
            return 0;
        ni->op = orig->op;
        ni->imm = orig->imm;
        ni->src1 = map_lookup(map, mcount, orig->src1);
        ni->src2 = map_lookup(map, mcount, orig->src2);
        ni->name = NULL;
        ni->data = NULL;
        ni->is_volatile = 0;
        ni->dest = (int)ir->next_value_id++;
        map[mcount++] = (map_entry_t){orig->dest, ni->dest, ni};
        pos = ni;
    }

    return 1;
}

/* Build an array containing all IR instructions in order */
static ir_instr_t **gather_call_list(ir_builder_t *ir, int *count)
{
    *count = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        (*count)++;
    if (*count == 0)
        return NULL;

    ir_instr_t **list = malloc((size_t)(*count) * sizeof(*list));
    if (!list) {
        opt_error("out of memory");
        return NULL;
    }

    int idx = 0;
    for (ir_instr_t *it = ir->head; it; it = it->next)
        list[idx++] = it;

    return list;
}

/* Inline a single call instruction if it matches an eligible function */
static int inline_call(ir_builder_t *ir, ir_instr_t **list, int *count, int i,
                       inline_func_t *funcs, size_t func_count)
{
    ir_instr_t *ins = list[i];
    if (ins->op != IR_CALL)
        return 0;

    inline_func_t *fn = NULL;
    for (size_t j = 0; j < func_count; j++) {
        if (strcmp(funcs[j].name, ins->name) == 0) {
            fn = &funcs[j];
            break;
        }
    }
    if (!fn || ins->imm != fn->param_count || i < (int)fn->param_count)
        return 0;

    int argc = (int)fn->param_count;
    if (argc > 8)
        return 0; /* limit to small functions */

    int args[8];
    if (!gather_call_args(list, i, argc, args))
        return 0;

    for (int a = 0; a < argc; a++) {
        remove_instr(ir, list, count, i - 1);
        i--;
    }

    int ret_val;
    if (!insert_inline_body(ir, ins, fn, argc, args, &ret_val))
        return 0;

    replace_value_uses(list, i + 1, *count, ins->dest, ret_val);

    free(ins->name);
    ins->name = NULL;
    remove_instr(ir, list, count, i);
    return 1;
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
    ir_instr_t **list = gather_call_list(ir, &count);
    if (!list) {
        for (size_t j = 0; j < func_count; j++)
            free(funcs[j].body);
        free(funcs);
        return;
    }

    for (int i = 0; i < count; i++) {
        if (inline_call(ir, list, &count, i, funcs, func_count))
            i--; /* restart from previous position after modification */
    }

    recompute_tail(ir);
    free(list);
    for (size_t j = 0; j < func_count; j++)
        free(funcs[j].body);
    free(funcs);
}


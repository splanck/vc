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
static int is_inline_def(const char *file, const char *name)
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
        size_t pre_len = (size_t)(pos - s);
        char *prefix = malloc(pre_len + 1);
        if (!prefix) {
            error_set(0, 0, file, name);
            error_print("out of memory");
            free(line);
            fclose(f);
            errno = ENOMEM;
            return -1;
        }
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
        free(prefix);
        break; /* processed definition line */
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
 * IR_LOAD_PARAM, IR_CONST and IR_RETURN are permitted.  The total number
 * of arithmetic operations must not exceed four.
 */
static int clone_func_body(ir_instr_t *begin, ir_instr_t **out, size_t *count)
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
        } else if (it->op != IR_LOAD_PARAM && it->op != IR_CONST && it->op != IR_RETURN) {
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
 * Identify inline functions with bodies accepted by clone_func_body.
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
        if (!clone_func_body(ins, &body, &body_count))
            continue;

        int param_count = 0;
        for (size_t i = 0; i < body_count; i++)
            if (body[i].op == IR_LOAD_PARAM && body[i].imm + 1 > param_count)
                param_count = body[i].imm + 1;

        /* Check that the defining line marks the function as inline */
        int is_inline = 0;
        const char *src_file = ins->next ? ins->next->file : NULL;
        if (src_file) {
            int r = is_inline_def(src_file, ins->name);
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

static int map_lookup(map_entry_t *map, size_t count, int old)
{
    for (size_t i = 0; i < count; i++)
        if (map[i].old_id == old)
            return map[i].new_id;
    return old;
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
        if (!fn || ins->imm != fn->param_count ||
            i < (int)fn->param_count)
            continue;

        int argc = (int)fn->param_count;
        int args[8];
        if (argc > 8)
            continue; /* limit to small functions */

        int ok = 1;
        for (int a = 0; a < argc; a++) {
            ir_instr_t *arg = list[i - argc + a];
            if (!arg || arg->op != IR_ARG) { ok = 0; break; }
            args[a] = arg->src1;
        }
        if (!ok)
            continue;

        for (int a = 0; a < argc; a++) {
            remove_instr(ir, list, &count, i-1);
            i--;
        }

        map_entry_t map[32];
        size_t mcount = 0;

        /* helper to resolve old value ids to newly created ones */
        int (*lookup)(map_entry_t *, size_t, int) = map_lookup;

        ir_instr_t *pos = ins;
        int ret_val = ins->dest;

        for (size_t k = 0; k < fn->count; k++) {
            ir_instr_t *orig = &fn->body[k];
            if (orig->op == IR_LOAD_PARAM) {
                if (orig->imm >= argc) { ok = 0; break; }
                map[mcount++] = (map_entry_t){orig->dest, args[orig->imm], NULL};
                continue;
            }
            if (orig->op == IR_RETURN) {
                ret_val = lookup(map, mcount, orig->src1);
                for (size_t m = 0; m < mcount; m++) {
                    if (map[m].old_id == orig->src1 && map[m].ins) {
                        map[m].ins->dest = ins->dest;
                        map[m].new_id = ins->dest;
                        ret_val = ins->dest;
                        break;
                    }
                }
                continue;
            }

            ir_instr_t *ni = ir_insert_after(ir, pos);
            if (!ni) { ok = 0; break; }
            ni->op = orig->op;
            ni->imm = orig->imm;
            ni->src1 = lookup(map, mcount, orig->src1);
            ni->src2 = lookup(map, mcount, orig->src2);
            ni->name = NULL;
            ni->data = NULL;
            ni->is_volatile = 0;
            ni->dest = (int)ir->next_value_id++;
            map[mcount++] = (map_entry_t){orig->dest, ni->dest, ni};
            pos = ni;
        }
        if (!ok)
            continue;

        for (int u = i + 1; u < count; u++) {
            ir_instr_t *uins = list[u];
            if (uins->src1 == ins->dest)
                uins->src1 = ret_val;
            if (uins->src2 == ins->dest)
                uins->src2 = ret_val;
        }

        free(ins->name);
        ins->name = NULL;
        remove_instr(ir, list, &count, i);
        i--;
    }

    recompute_tail(ir);
    free(list);
    for (size_t j = 0; j < func_count; j++)
        free(funcs[j].body);
    free(funcs);
}


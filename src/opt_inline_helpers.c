/*
 * Helper routines shared by the inline optimizer.
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
#include "opt_inline_helpers.h"

/* --- local helpers for parsing inline hints ----------------------------- */

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

/* --- exported helpers --------------------------------------------------- */

int clone_inline_body(ir_instr_t *begin, ir_instr_t **out, size_t *count)
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

int collect_funcs(ir_builder_t *ir, inline_func_t **out, size_t *count)
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


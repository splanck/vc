#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <string.h>
#include "preproc_paste.h"
#include "preproc_macro_utils.h"
#include "strbuf.h"
#include "util.h"

static void trim_trailing_ws(strbuf_t *sb)
{
    while (sb->len > 0 &&
           (sb->data[sb->len - 1] == ' ' || sb->data[sb->len - 1] == '\t')) {
        sb->len--;
        sb->data[sb->len] = '\0';
    }
}

static int append_pasted_tokens(const char *value, size_t i, size_t len,
                                const char *rep, const vector_t *params,
                                char **args, int variadic, strbuf_t *sb,
                                size_t *out_i)
{
    size_t k = i + len;
    while (value[k] == ' ' || value[k] == '\t')
        k++;
    if (value[k] != '#' || value[k + 1] != '#') {
        size_t j = i;
        while (j >= 2 && (value[j-1]==' ' || value[j-1]=='\t'))
            j--;
        if (j >= 2 && value[j-2]=='#' && value[j-1]=='#') {
            trim_trailing_ws(sb);
            if (sb->len >= 2 && sb->data[sb->len-2]=='#' && sb->data[sb->len-1]=='#') {
                sb->len -= 2;
                sb->data[sb->len] = '\0';
            }
        } else {
            return 0;
        }
    } else {
        k += 2;
        while (value[k] == ' ' || value[k] == '\t')
            k++;
    }

    if (!value[k]) {
        trim_trailing_ws(sb);
        if (rep)
            strbuf_append(sb, rep);
        else
            strbuf_appendf(sb, "%.*s", (int)len, value + i);
        *out_i = k;
    } else if (isalpha((unsigned char)value[k]) || value[k] == '_') {
        size_t l = parse_ident(value + k);
        const char *rep2 = lookup_param(value + k, l, params, args);
        if (!rep2 && variadic && l == 11 &&
            strncmp(value + k, "__VA_ARGS__", 11) == 0)
            rep2 = args[params->count];
        trim_trailing_ws(sb);
        if (rep)
            strbuf_append(sb, rep);
        else
            strbuf_appendf(sb, "%.*s", (int)len, value + i);
        if (rep2)
            strbuf_append(sb, rep2);
        else
            strbuf_appendf(sb, "%.*s", (int)l, value + k);
        *out_i = k + l;
    } else {
        trim_trailing_ws(sb);
        if (rep)
            strbuf_append(sb, rep);
        else
            strbuf_appendf(sb, "%.*s", (int)len, value + i);
        strbuf_appendf(sb, "%c", value[k]);
        *out_i = k + 1;
    }
    return 1;
}

char *expand_params(const char *value, const vector_t *params, char **args,
                    int variadic)
{
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; value[i];) {
        if (value[i] == '#' && value[i + 1] != '#') {
            i = append_stringized_param(value, i, params, args, variadic, &sb);
            continue;
        }
        if (value[i] == '#' && value[i + 1] == '#') {
            strbuf_append(&sb, "##");
            i += 2;
            while (value[i] == ' ' || value[i] == '\t') {
                strbuf_appendf(&sb, "%c", value[i]);
                i++;
            }
            continue;
        }
        size_t len = parse_ident(value + i);
        if (len) {
            const char *rep = lookup_param(value + i, len, params, args);
            if (!rep && variadic && len == 11 &&
                strncmp(value + i, "__VA_ARGS__", 11) == 0)
                rep = args[params->count];
            size_t next;
            if (append_pasted_tokens(value, i, len, rep, params, args, variadic,
                                     &sb, &next)) {
                i = next;
                continue;
            }
            if (rep)
                strbuf_append(&sb, rep);
            else
                strbuf_appendf(&sb, "%.*s", (int)len, value + i);
            i += len;
            continue;
        }
        strbuf_appendf(&sb, "%c", value[i]);
        i++;
    }
    char *out = vc_strdup(sb.data ? sb.data : "");
    strbuf_free(&sb);
    if (!out)
        vc_oom();
    return out;
}

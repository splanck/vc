#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <string.h>
#include "preproc_macro_utils.h"

size_t parse_ident(const char *s)
{
    size_t i = 0;
    if (!isalpha((unsigned char)s[i]) && s[i] != '_')
        return 0;
    i++;
    while (isalnum((unsigned char)s[i]) || s[i] == '_')
        i++;
    return i;
}

const char *lookup_param(const char *name, size_t len,
                         const vector_t *params, char **args)
{
    for (size_t p = 0; p < params->count; p++) {
        const char *param = ((char **)params->data)[p];
        if (strlen(param) == len && strncmp(param, name, len) == 0)
            return args[p];
    }
    return NULL;
}

size_t append_stringized_param(const char *value, size_t i,
                               const vector_t *params, char **args,
                               int variadic, strbuf_t *sb)
{
    size_t j = i + 1;
    while (value[j] == ' ' || value[j] == '\t')
        j++;
    size_t len = parse_ident(value + j);
    if (len) {
        const char *rep = lookup_param(value + j, len, params, args);
        if (!rep && variadic && len == 11 &&
            strncmp(value + j, "__VA_ARGS__", 11) == 0)
            rep = args[params->count];
        if (rep) {
            strbuf_append(sb, "\"");
            for (size_t k = 0; rep[k]; k++) {
                char c = rep[k];
                if (c == '\\' || c == '"')
                    strbuf_appendf(sb, "\\%c", c);
                else
                    strbuf_appendf(sb, "%c", c);
            }
            strbuf_append(sb, "\"");
            return j + len;
        }
    }
    strbuf_append(sb, "#");
    return i + 1;
}

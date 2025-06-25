/*
 * Dynamic string buffer implementation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "strbuf.h"

/* Initialize a string buffer */
void strbuf_init(strbuf_t *sb)
{
    if (!sb)
        return;
    sb->cap = 128;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (sb->data)
        sb->data[0] = '\0';
}

/* Ensure the buffer can hold at least extra bytes */
static void sb_ensure(strbuf_t *sb, size_t extra)
{
    if (!sb->data || sb->len + extra >= sb->cap) {
        size_t new_cap = sb->cap ? sb->cap * 2 : 128;
        while (sb->len + extra >= new_cap)
            new_cap *= 2;
        char *n = realloc(sb->data, new_cap);
        if (!n)
            return;
        sb->data = n;
        sb->cap = new_cap;
    }
}

/* Append a simple string to the buffer */
void strbuf_append(strbuf_t *sb, const char *text)
{
    if (!sb || !text)
        return;
    size_t l = strlen(text);
    sb_ensure(sb, l + 1);
    if (!sb->data)
        return;
    memcpy(sb->data + sb->len, text, l + 1);
    sb->len += l;
}

/* Append formatted text using printf-style formatting */
void strbuf_appendf(strbuf_t *sb, const char *fmt, ...)
{
    if (!sb || !fmt)
        return;
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
        strbuf_append(sb, tmp);
        free(tmp);
    } else {
        strbuf_append(sb, buf);
    }
}

/* Free the memory used by a string buffer */
void strbuf_free(strbuf_t *sb)
{
    if (!sb)
        return;
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

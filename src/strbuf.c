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
#include <stdint.h>
#include <errno.h>
#include "strbuf.h"
#include "util.h"

/*
 * Initialise an empty string buffer.  The buffer grows automatically as
 * text is appended.
 */
void strbuf_init(strbuf_t *sb)
{
    if (!sb)
        return;
    sb->cap = 128;
    sb->len = 0;
    sb->data = vc_alloc_or_exit(sb->cap);
    sb->data[0] = '\0';
}

/* Ensure the buffer can hold at least "extra" additional bytes. */
/*
 * Ensure the buffer can hold at least "extra" additional bytes.
 * Returns 0 on success and -1 on failure.  On failure an error message
 * is printed and no reallocation is performed.
 */
static int sb_ensure(strbuf_t *sb, size_t extra)
{
    if (!sb)
        return -1;

    /* calculate required capacity and detect overflow */
    if (sb->len > SIZE_MAX - extra - 1) {
        fprintf(stderr, "vc: string buffer too large\n");
        return -1;
    }
    size_t need = sb->len + extra + 1;

    if (!sb->data || need > sb->cap) {
        size_t new_cap = sb->cap ? sb->cap : 128;
        while (new_cap < need) {
            if (new_cap > SIZE_MAX / 2) {
                fprintf(stderr, "vc: string buffer too large\n");
                return -1;
            }
            new_cap *= 2;
        }
        char *n = vc_realloc_or_exit(sb->data, new_cap);
        if (!n)
            return -1;
        sb->data = n;
        sb->cap = new_cap;
    }
    return 0;
}

/* Append a simple NUL terminated string to the buffer. */
/*
 * Append a simple NUL terminated string to the buffer.
 * Returns 0 on success and -1 on failure.
 */
int strbuf_append(strbuf_t *sb, const char *text)
{
    if (!sb || !text)
        return -1;
    size_t l = strlen(text);
    if (sb_ensure(sb, l + 1) < 0)
        return -1;
    if (!sb->data)
        return -1;
    memcpy(sb->data + sb->len, text, l + 1);
    sb->len += l;
    return 0;
}

/* Append formatted text using printf-style formatting. */
/*
 * Append formatted text using printf-style formatting. Returns 0 on success
 * and -1 on failure.
 */
int strbuf_appendf(strbuf_t *sb, const char *fmt, ...)
{
    if (!sb || !fmt)
        return -1;
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (ret < 0) {
        if (errno == EOVERFLOW) {
            fprintf(stderr, "vc: formatted output too large\n");
            return -1;
        }
        return -1;
    }
    size_t n = (size_t)ret;
    if (n >= sizeof(buf)) {
        if (n > SIZE_MAX - 1) {
            fprintf(stderr, "vc: formatted output too large\n");
            return -1;
        }
        char *tmp = vc_alloc_or_exit(n + 1);
        va_start(ap, fmt);
        ret = vsnprintf(tmp, n + 1, fmt, ap);
        va_end(ap);
        if (ret < 0) {
            free(tmp);
            if (errno == EOVERFLOW) {
                fprintf(stderr, "vc: formatted output too large\n");
                return -1;
            }
            return -1;
        }
        int rc = strbuf_append(sb, tmp);
        free(tmp);
        return rc;
    }
    return strbuf_append(sb, buf);
}

/* Free the memory used by a string buffer. */
void strbuf_free(strbuf_t *sb)
{
    if (!sb)
        return;
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

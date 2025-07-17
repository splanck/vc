/*
 * Dynamic string buffer API.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STRBUF_H
#define VC_STRBUF_H

#include <stddef.h>

/* Simple dynamic string buffer utility */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf_t;

/* Initialize a new string buffer */
void strbuf_init(strbuf_t *sb);

/*
 * Append a plain string to the buffer.
 *
 * Returns 0 on success and -1 on failure.  An allocation failure results
 * in a -1 return with the buffer left unchanged.
 */
int strbuf_append(strbuf_t *sb, const char *text);

/*
 * Append formatted text using printf-style formatting.
 *
 * Returns 0 on success and -1 on failure.  Allocation failures yield a -1
 * return with the buffer left unchanged.
 */
int strbuf_appendf(strbuf_t *sb, const char *fmt, ...);

/* Release buffer memory */
void strbuf_free(strbuf_t *sb);

#endif /* VC_STRBUF_H */

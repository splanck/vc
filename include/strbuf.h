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

void strbuf_init(strbuf_t *sb);
void strbuf_append(strbuf_t *sb, const char *text);
void strbuf_appendf(strbuf_t *sb, const char *fmt, ...);
void strbuf_free(strbuf_t *sb);

#endif /* VC_STRBUF_H */

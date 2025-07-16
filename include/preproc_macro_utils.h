/*
 * Macro parsing and stringizing helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_MACRO_UTILS_H
#define VC_PREPROC_MACRO_UTILS_H

#include "vector.h"
#include "strbuf.h"

size_t parse_ident(const char *s);
const char *lookup_param(const char *name, size_t len,
                         const vector_t *params, char **args);
size_t append_stringized_param(const char *value, size_t i,
                               const vector_t *params, char **args,
                               int variadic, strbuf_t *sb);

#endif /* VC_PREPROC_MACRO_UTILS_H */

/*
 * Builtin macro handling utilities.
 */

#ifndef VC_PREPROC_BUILTIN_H
#define VC_PREPROC_BUILTIN_H

#include "strbuf.h"
#include "preproc_file.h"

void preproc_set_location(preproc_context_t *ctx, const char *file,
                          size_t line, size_t column);
void preproc_set_function(preproc_context_t *ctx, const char *name);
void preproc_set_base_file(preproc_context_t *ctx, const char *file);
void preproc_set_include_level(preproc_context_t *ctx, size_t level);
size_t preproc_get_line(const preproc_context_t *ctx);
size_t preproc_get_column(const preproc_context_t *ctx);
int handle_builtin_macro(const char *name, size_t len, size_t end,
                         size_t column, strbuf_t *out, size_t *pos,
                         preproc_context_t *ctx);

#endif /* VC_PREPROC_BUILTIN_H */

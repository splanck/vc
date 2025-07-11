/*
 * Builtin macro handling utilities.
 */

#ifndef VC_PREPROC_BUILTIN_H
#define VC_PREPROC_BUILTIN_H

#include "strbuf.h"

void preproc_set_location(const char *file, size_t line, size_t column);
void preproc_set_function(const char *name);
void preproc_set_base_file(const char *file);
void preproc_set_include_level(size_t level);
size_t preproc_get_line(void);
size_t preproc_get_column(void);
int handle_builtin_macro(const char *name, size_t len, size_t end,
                         size_t column, strbuf_t *out, size_t *pos);

#endif /* VC_PREPROC_BUILTIN_H */

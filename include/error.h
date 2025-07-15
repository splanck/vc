/*
 * Error reporting helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_ERROR_H
#define VC_ERROR_H

#include <stddef.h>
#include <stdbool.h>

typedef struct error_context {
    const char *file;
    const char *function;
    size_t line;
    size_t column;
} error_context_t;

/*
 * Store the given source position along with the active file and
 * current function name. The file and function parameters may be
 * NULL to leave the previous values unchanged.
 */
void error_set(error_context_t *ctx, size_t line, size_t col,
               const char *file, const char *func);

/*
 * Print an error message to stderr using the position stored in
 * the given context.
 */
void error_print(const error_context_t *ctx, const char *msg);

/* Current diagnostic context */
extern error_context_t error_ctx;
extern bool error_use_color;

#endif /* VC_ERROR_H */

/*
 * Helpers to dump the AST as a human readable string.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_AST_DUMP_H
#define VC_AST_DUMP_H

#include "ast.h"

/*
 * Convert the parsed AST into a textual representation.
 * The returned buffer must be freed by the caller.
 */
char *ast_to_string(func_t **funcs, size_t fcount,
                    stmt_t **globs, size_t gcount);

#endif /* VC_AST_DUMP_H */

/*
 * Token pasting and parameter expansion helpers.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_PASTE_H
#define VC_PREPROC_PASTE_H

#include "vector.h"
#include "strbuf.h"

char *expand_params(const char *value, const vector_t *params,
                    char **args, int variadic);

#endif /* VC_PREPROC_PASTE_H */

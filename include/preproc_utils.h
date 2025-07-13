/*
 * Common helper functions for the preprocessor.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_UTILS_H
#define VC_PREPROC_UTILS_H

#include <ctype.h>

/* Advance P past whitespace and return the updated pointer */
static inline char *skip_ws(char *p)
{
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

#endif /* VC_PREPROC_UTILS_H */

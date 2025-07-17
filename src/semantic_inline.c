/*
 * Inline function emission tracking.
 * Maintains a list of functions whose definitions have
 * already been output so inline copies are emitted only once.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "semantic_inline.h"
#include "util.h"

/* Track inline functions already emitted */
static const char **inline_emitted = NULL;
static size_t inline_emitted_count = 0;

/* Check if an inline function name was recorded */
static int inline_already_emitted(const char *name)
{
    for (size_t i = 0; i < inline_emitted_count; i++)
        if (strcmp(inline_emitted[i], name) == 0)
            return 1;
    return 0;
}

/* Record an emitted inline function name */
static int mark_inline_emitted(const char *name)
{
    const char **tmp = realloc((void *)inline_emitted,
                               (inline_emitted_count + 1) * sizeof(*tmp));
    if (!tmp)
        return 0;
    inline_emitted = tmp;
    inline_emitted[inline_emitted_count] = vc_strdup(name ? name : "");
    if (!inline_emitted[inline_emitted_count])
        return 0;
    inline_emitted_count++;
    return 1;
}

int semantic_inline_already_emitted(const char *name)
{
    return inline_already_emitted(name);
}

int semantic_mark_inline_emitted(const char *name)
{
    return mark_inline_emitted(name);
}

void semantic_inline_cleanup(void)
{
    for (size_t i = 0; i < inline_emitted_count; i++)
        free((void *)inline_emitted[i]);
    free((void *)inline_emitted);
    inline_emitted = NULL;
    inline_emitted_count = 0;
}


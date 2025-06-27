/*
 * Manage numeric labels for code generation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "label.h"
#include <stdio.h>

/* Next label identifier to issue. */
static int next_label_id = 0;

/*
 * Initialise the label module.  Future calls to label_next_id() will
 * start at zero after this function is called.
 */
void label_init(void)
{
    next_label_id = 0;
}

/*
 * Return a unique integer label identifier.  Identifiers are issued in
 * increasing order starting from zero.
 */
int label_next_id(void)
{
    return next_label_id++;
}

/* Reset the counter so a new compilation can start from label zero. */
void label_reset(void)
{
    next_label_id = 0;
}

/* Format a label combining prefix and id.  "buf" must have space for 32 bytes. */
const char *label_format(const char *prefix, int id, char buf[32])
{
    snprintf(buf, 32, "%s%d", prefix, id);
    return buf;
}

/* Like label_format() but appends an additional suffix string. */
const char *label_format_suffix(const char *prefix, int id, const char *suffix,
                                char buf[32])
{
    snprintf(buf, 32, "%s%d%s", prefix, id, suffix);
    return buf;
}

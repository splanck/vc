/*
 * Manage numeric labels for code generation.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "label.h"

/* Next label identifier to issue */
static int next_label_id = 0;

/* Reset label counter to start at zero */
void label_init(void)
{
    next_label_id = 0;
}

/* Return a fresh label identifier */
int label_next_id(void)
{
    return next_label_id++;
}

/* Clear label state so a new compilation can start */
void label_reset(void)
{
    next_label_id = 0;
}

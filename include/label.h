/*
 * Simple label ID generator.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_LABEL_H
#define VC_LABEL_H

void label_init(void);
int label_next_id(void);
void label_reset(void);

#endif /* VC_LABEL_H */

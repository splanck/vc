/*
 * Simple label ID generator.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_LABEL_H
#define VC_LABEL_H

/* Initialize the label generator. */
void label_init(void);

/* Get the next unique label identifier. */
int label_next_id(void);

/* Reset label numbering back to zero. */
void label_reset(void);

#endif /* VC_LABEL_H */

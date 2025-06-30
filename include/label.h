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

/*
 * Format a label as prefix followed by id.  Returns NULL and reports an
 * error if the result would exceed 31 characters.  On failure, the
 * provided buffer's first byte is set to '\0'.
 */
const char *label_format(const char *prefix, int id, char buf[32]);

/*
 * Format a label as prefix + id + suffix.  Returns NULL and reports an error
 * if the result would exceed 31 characters.  On failure, the provided
 * buffer's first byte is set to '\0'.
 */
const char *label_format_suffix(const char *prefix, int id, const char *suffix,
                                char buf[32]);

#endif /* VC_LABEL_H */

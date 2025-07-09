/*
 * Inline function emission tracking helpers.
 * Provides routines to record and query which inline
 * functions have been emitted.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_SEMANTIC_INLINE_H
#define VC_SEMANTIC_INLINE_H

/* Return 1 if an inline function's definition was already emitted */
int semantic_inline_already_emitted(const char *name);

/* Mark an inline function as emitted.  Returns 0 on allocation failure */
int semantic_mark_inline_emitted(const char *name);

/* Free resources used to track emitted inline functions */
void semantic_inline_cleanup(void);

#endif /* VC_SEMANTIC_INLINE_H */

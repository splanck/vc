/*
 * File processing entry points for the preprocessor.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_PREPROC_FILE_H
#define VC_PREPROC_FILE_H

#include "vector.h"

/* Preprocess the file at the given path.
 * The returned string must be freed by the caller.
 * Returns NULL on failure.
 */
char *preproc_run(const char *path, const vector_t *include_dirs);

#endif /* VC_PREPROC_FILE_H */

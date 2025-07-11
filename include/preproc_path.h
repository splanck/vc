/*
 * Path and include stack helpers for the preprocessor.
 */
#ifndef VC_PREPROC_PATH_H
#define VC_PREPROC_PATH_H

#include "vector.h"
#include "preproc_file.h"

int record_dependency(preproc_context_t *ctx, const char *path);
int pragma_once_contains(preproc_context_t *ctx, const char *path);
int pragma_once_add(preproc_context_t *ctx, const char *path);
char *find_include_path(const char *fname, char endc, const char *dir,
                        const vector_t *incdirs, size_t start, size_t *out_idx);
int append_env_paths(const char *env, vector_t *search_dirs);
int collect_include_dirs(vector_t *search_dirs,
                         const vector_t *include_dirs);

#endif /* VC_PREPROC_PATH_H */

/*
 * Path and include stack helpers for the preprocessor.
 */
#ifndef VC_PREPROC_PATH_H
#define VC_PREPROC_PATH_H

#include <stdio.h>
#include <stdbool.h>
#include "vector.h"
#include "preproc_file.h"

int record_dependency(preproc_context_t *ctx, const char *path);
int pragma_once_contains(preproc_context_t *ctx, const char *path);
int pragma_once_add(preproc_context_t *ctx, const char *path);
char *find_include_path(const char *fname, char endc, const char *dir,
                        const vector_t *incdirs, size_t start, size_t *out_idx);
int append_env_paths(const char *env, vector_t *search_dirs);
int collect_include_dirs(vector_t *search_dirs,
                         const vector_t *include_dirs,
                         const char *sysroot,
                         const char *vc_sysinclude,
                         bool internal_libc);

/* Print the directories searched for an include directive */
void print_include_search_dirs(FILE *fp, char endc, const char *dir,
                               const vector_t *incdirs, size_t start);

/* Release cached include path resources */
void preproc_path_cleanup(void);

#endif /* VC_PREPROC_PATH_H */

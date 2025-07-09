/*
 * Path and include stack helpers for the preprocessor.
 */
#ifndef VC_PREPROC_PATH_H
#define VC_PREPROC_PATH_H

#include "vector.h"
#include "strbuf.h"
#include "preproc_file.h"

/* Entry on the include stack */
typedef struct {
    char *path;
    size_t dir_index;
} include_entry_t;

int record_dependency(preproc_context_t *ctx, const char *path);
int include_stack_contains(vector_t *stack, const char *path);
int include_stack_push(vector_t *stack, const char *path, size_t idx);
void include_stack_pop(vector_t *stack);
int pragma_once_contains(preproc_context_t *ctx, const char *path);
int pragma_once_add(preproc_context_t *ctx, const char *path);
char *find_include_path(const char *fname, char endc, const char *dir,
                        const vector_t *incdirs, size_t start, size_t *out_idx);
char *read_file_lines(const char *path, char ***out_lines);
int load_file_lines(const char *path, char ***out_lines,
                    char **out_dir, char **out_text);
int load_and_register_file(const char *path, vector_t *stack, size_t idx,
                           char ***out_lines, char **out_dir, char **out_text,
                           preproc_context_t *ctx);
int append_env_paths(const char *env, vector_t *search_dirs);
int collect_include_dirs(vector_t *search_dirs,
                         const vector_t *include_dirs);

#endif /* VC_PREPROC_PATH_H */

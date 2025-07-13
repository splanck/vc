/*
 * File loading and include stack helpers for the preprocessor.
 */
#ifndef VC_PREPROC_FILE_IO_H
#define VC_PREPROC_FILE_IO_H

#include "vector.h"
#include "strbuf.h"
#include "preproc_file.h"

/* Entry on the include stack */
typedef struct {
    char *path;
    size_t dir_index;
    int prev_system_header;
} include_entry_t;

int include_stack_contains(vector_t *stack, const char *path);
int include_stack_push(vector_t *stack, const char *path, size_t idx,
                       preproc_context_t *ctx);
void include_stack_pop(vector_t *stack, preproc_context_t *ctx);

char *read_file_lines(const char *path, char ***out_lines);
int load_file_lines(const char *path, char ***out_lines,
                    char **out_dir, char **out_text);
int load_and_register_file(const char *path, vector_t *stack, size_t idx,
                           char ***out_lines, char **out_dir, char **out_text,
                           preproc_context_t *ctx);

int process_all_lines(char **lines, const char *path, const char *dir,
                      vector_t *macros, vector_t *conds, strbuf_t *out,
                      const vector_t *incdirs, vector_t *stack,
                      preproc_context_t *ctx);
void preproc_apply_line_directive(preproc_context_t *ctx,
                                  const char *file, int line);

void cleanup_file_resources(char *text, char **lines, char *dir);

void line_state_push(preproc_context_t *ctx, const char *file, long delta,
                     char **prev_file, long *prev_delta);
void line_state_pop(preproc_context_t *ctx, char *prev_file, long prev_delta);

#endif /* VC_PREPROC_FILE_IO_H */

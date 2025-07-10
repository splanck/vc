/*
 * Helper functions for macro argument processing.
 */

#ifndef VC_PREPROC_ARGS_H
#define VC_PREPROC_ARGS_H

#include "vector.h"

int parse_macro_args(const char *line, size_t *pos, vector_t *out);
int gather_varargs(vector_t *args, size_t fixed,
                   char ***out_ap, char **out_va);
int parse_macro_arguments(const char *line, size_t *pos, vector_t *out,
                          size_t param_count, int variadic);
int handle_varargs(vector_t *args, size_t fixed, int variadic,
                   char ***out_ap, char **out_va);
void free_arg_vector(vector_t *v);

#endif /* VC_PREPROC_ARGS_H */

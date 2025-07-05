#ifndef VC_PREPROC_INCLUDE_H
#define VC_PREPROC_INCLUDE_H

#include "vector.h"

char *find_include_path(const char *fname, char endc,
                        const char *dir, const vector_t *incdirs);
int include_stack_contains(vector_t *stack, const char *path);
int include_stack_push(vector_t *stack, const char *path);
void include_stack_pop(vector_t *stack);
int pragma_once_contains(const char *path);
int pragma_once_add(const char *path);
void preproc_include_init(void);
void preproc_include_cleanup(void);

#endif /* VC_PREPROC_INCLUDE_H */

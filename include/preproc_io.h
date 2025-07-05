#ifndef VC_PREPROC_IO_H
#define VC_PREPROC_IO_H

#include "vector.h"

int load_source(const char *path, vector_t *stack,
                char ***out_lines, char **out_dir, char **out_text);
void cleanup_source(char *text, char **lines, char *dir);

#endif /* VC_PREPROC_IO_H */

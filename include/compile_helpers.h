#ifndef VC_COMPILE_HELPERS_H
#define VC_COMPILE_HELPERS_H

#include "cli.h"

/*
 * Create a temporary file and return its descriptor.  On success the path
 * is stored in *out_path.  Returns -1 on failure with errno set to one of:
 *   ENAMETOOLONG - path would exceed PATH_MAX or snprintf truncated
 *   others       - from malloc, mkstemp or fcntl
 */
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);

/* Generate an object filename from the given source path. Caller frees result. */
char *vc_obj_name(const char *source);

/* Retrieve the C compiler command path. */
const char *get_cc(void);

/* Retrieve the assembler command path. */
const char *get_as(int intel);

#endif /* VC_COMPILE_HELPERS_H */

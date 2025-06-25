/*
 * Command line option structures.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_CLI_H
#define VC_CLI_H

#include "opt.h"
#include "vector.h"

/* Command line options parsed from argv */
typedef struct {
    char *output;       /* output file path */
    opt_config_t opt_cfg; /* optimization configuration */
    int use_x86_64;     /* enable 64-bit codegen */
    int compile;        /* assemble to object */
    int link;           /* build executable */
    int dump_asm;       /* dump assembly to stdout */
    int dump_ir;        /* dump IR to stdout */
    vector_t include_dirs; /* additional include directories */
    const char *source; /* input source file */
} cli_options_t;

/* Parse command line arguments. Returns 0 on success, non-zero on error. */
int cli_parse_args(int argc, char **argv, cli_options_t *opts);

#endif /* VC_CLI_H */

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

typedef enum {
    STD_C99 = 0,
    STD_GNU99
} c_std_t;

/* Command line options parsed from argv */
typedef struct {
    char *output;       /* output file path */
    opt_config_t opt_cfg; /* optimization configuration */
    int use_x86_64;     /* enable 64-bit codegen */
    int compile;        /* assemble to object */
    int link;           /* build executable */
    int dump_asm;       /* dump assembly to stdout (-S/--dump-asm) */
    int dump_ir;        /* dump IR to stdout */
    int preprocess;     /* run preprocessor only and print to stdout */
    c_std_t std;        /* language standard */
    vector_t include_dirs; /* additional include directories */
    vector_t sources;      /* input source files */
} cli_options_t;

/* Parse command line arguments. Returns 0 on success, non-zero on error. */
int cli_parse_args(int argc, char **argv, cli_options_t *opts);

#endif /* VC_CLI_H */

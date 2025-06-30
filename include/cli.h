/*
 * Definitions for the vc command line interface.
 *
 * This header declares the cli_options_t structure used by
 * the compiler and the function for parsing argv.
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

typedef enum {
    ASM_ATT = 0,
    ASM_INTEL
} asm_syntax_t;

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
    int debug;          /* emit debug directives */
    asm_syntax_t asm_syntax; /* assembly syntax flavor */
    c_std_t std;        /* language standard */
    char *obj_dir;      /* directory for temporary object files */
    vector_t include_dirs; /* additional include directories */
    vector_t sources;      /* input source files */
} cli_options_t;

/* Parse command line arguments. Returns 0 on success, non-zero on error. */
int cli_parse_args(int argc, char **argv, cli_options_t *opts);

#endif /* VC_CLI_H */

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
#include <stdbool.h>

typedef enum {
    STD_C99 = 0,
    STD_GNU99
} c_std_t;

typedef enum {
    ASM_ATT = 0,
    ASM_INTEL
} asm_syntax_t;

/* getopt_long option identifiers */
typedef enum {
    CLI_OPT_NO_FOLD = 1,
    CLI_OPT_NO_DCE,
    CLI_OPT_X86_64,
    CLI_OPT_DUMP_ASM_LONG,
    CLI_OPT_NO_CPROP,
    CLI_OPT_DUMP_AST,
    CLI_OPT_DUMP_IR,
    CLI_OPT_LINK = 8,
    CLI_OPT_STD,
    CLI_OPT_OBJ_DIR,
    CLI_OPT_DEBUG = 11,
    CLI_OPT_NO_INLINE,
    CLI_OPT_INTEL_SYNTAX = 13,
    CLI_OPT_DEFINE,
    CLI_OPT_UNDEFINE,
    CLI_OPT_NO_COLOR,
    CLI_OPT_DUMP_TOKENS,
    CLI_OPT_DEP_ONLY,
    CLI_OPT_DEP,
    CLI_OPT_NO_WARN_UNREACHABLE,
    CLI_OPT_EMIT_DWARF,
    CLI_OPT_FMAX_DEPTH,
    CLI_OPT_SYSROOT,
    CLI_OPT_VC_SYSINCLUDE,
    CLI_OPT_INTERNAL_LIBC
} cli_opt_id;

/* Command line options parsed from argv */
typedef struct {
    char *output;       /* output file path */
    opt_config_t opt_cfg; /* optimization configuration */
    bool use_x86_64;    /* enable 64-bit codegen */
    bool compile;       /* assemble to object */
    bool link;          /* build executable */
    bool dump_asm;      /* dump assembly to stdout (-S/--dump-asm) */
    bool dump_ast;      /* dump AST to stdout */
    bool dump_ir;       /* dump IR to stdout */
    bool dump_tokens;   /* dump token list to stdout */
    bool preprocess;    /* run preprocessor only and print to stdout */
    bool debug;         /* emit debug directives */
    bool emit_dwarf;    /* emit DWARF line and symbol data */
    bool color_diag;    /* use ANSI colors in diagnostics */
    bool dep_only;      /* generate dependencies only */
    bool deps;          /* generate dependency file */
    bool warn_unreachable; /* warn on unreachable statements */
    asm_syntax_t asm_syntax; /* assembly syntax flavor */
    c_std_t std;        /* language standard */
    char *obj_dir;      /* directory for temporary object files */
    char *sysroot;      /* prefix for system include paths */
    char *vc_sysinclude; /* first system include directory */
    bool internal_libc; /* use bundled libc */
    vector_t include_dirs; /* additional include directories */
    vector_t sources;      /* input source files */
    vector_t defines;      /* command line macro definitions */
    vector_t undefines;    /* macros to undefine before compilation */
    vector_t lib_dirs;     /* additional library search paths */
    vector_t libs;         /* libraries to link against */
    size_t max_include_depth; /* maximum nested includes */
} cli_options_t;

/* Parse command line arguments. Returns 0 on success, non-zero on error. */
int cli_parse_args(int argc, char **argv, cli_options_t *opts);

/* Free option vectors allocated by cli_parse_args */
void cli_free_opts(cli_options_t *opts);

#endif /* VC_CLI_H */

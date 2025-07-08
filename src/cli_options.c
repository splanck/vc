#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "cli_options.h"
#include "cli.h"
#define VERSION "0.1.0"
#include "util.h"

/* Forward declaration for usage printing implemented in cli.c */
void print_usage(const char *prog);

/* Add an include directory to "opts->include_dirs". Returns 0 on success */
static int add_include_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->include_dirs, &dir)) {
        vc_oom();
        return -1;
    }
    return 0;
}

/* Add a library search directory to "opts->lib_dirs". */
static int add_lib_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->lib_dirs, &dir)) {
        vc_oom();
        return -1;
    }
    return 0;
}

/* Add a library name to "opts->libs". */
static int add_library(cli_options_t *opts, const char *name)
{
    if (!vector_push(&opts->libs, &name)) {
        vc_oom();
        return -1;
    }
    return 0;
}

/* Set the optimization level and toggle individual passes accordingly. */
static int set_opt_level(cli_options_t *opts, const char *level)
{
    errno = 0;
    char *end;
    long long val = strtoll(level, &end, 10);
    if (*end != '\0' || errno != 0 || val < 0 || val > INT_MAX || val > 3) {
        fprintf(stderr, "Invalid optimization level '%s'\n", level);
        return 1;
    }

    opts->opt_cfg.opt_level = (int)val;
    if (opts->opt_cfg.opt_level <= 0) {
        opts->opt_cfg.fold_constants = 0;
        opts->opt_cfg.dead_code = 0;
        opts->opt_cfg.const_prop = 0;
        opts->opt_cfg.inline_funcs = 0;
    } else {
        opts->opt_cfg.fold_constants = 1;
        opts->opt_cfg.dead_code = 1;
        opts->opt_cfg.const_prop = 1;
        opts->opt_cfg.inline_funcs = 1;
    }

    return 0;
}

/* Parse and set the language standard string. */
static int set_standard(cli_options_t *opts, const char *std)
{
    if (strcmp(std, "c99") == 0)
        opts->std = STD_C99;
    else if (strcmp(std, "gnu99") == 0)
        opts->std = STD_GNU99;
    else {
        fprintf(stderr, "Unknown standard '%s'\n", std);
        return 1;
    }
    return 0;
}

/* Individual option handlers used by the parsing helpers. */
int handle_help(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)opts;
    print_usage(prog);
    exit(0);
}

int handle_version(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog; (void)opts;
    printf("vc version %s\n", VERSION);
    exit(0);
}

int add_define_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -D option\n");
        return 1;
    }
    if (!vector_push(&opts->defines, &arg)) {
        vc_oom();
        return 1;
    }
    return 0;
}

int add_undef_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -U option\n");
        return 1;
    }
    if (!vector_push(&opts->undefines, &arg)) {
        vc_oom();
        return 1;
    }
    return 0;
}

static int handle_std(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    return set_standard(opts, arg);
}

/* Parse options controlling optimization settings. */
int parse_optimization_opts(int opt, const char *arg, cli_options_t *opts)
{
    switch (opt) {
    case 'O':
        return set_opt_level(opts, arg);
    case CLI_OPT_NO_FOLD:
        opts->opt_cfg.fold_constants = 0;
        return 0;
    case CLI_OPT_NO_DCE:
        opts->opt_cfg.dead_code = 0;
        return 0;
    case CLI_OPT_NO_CPROP:
        opts->opt_cfg.const_prop = 0;
        return 0;
    case CLI_OPT_NO_INLINE:
        opts->opt_cfg.inline_funcs = 0;
        return 0;
    default:
        return -1;
    }
}

/* Parse options related to I/O paths such as includes and libraries. */
int parse_io_paths(int opt, const char *arg, cli_options_t *opts)
{
    switch (opt) {
    case 'o':
        opts->output = (char *)arg;
        return 0;
    case 'I':
        return add_include_dir(opts, arg);
    case 'L':
        return add_lib_dir(opts, arg);
    case 'l':
        return add_library(opts, arg);
    case CLI_OPT_OBJ_DIR:
        opts->obj_dir = (char *)arg;
        return 0;
    default:
        return -1;
    }
}

/* Parse all remaining command line options. */
int parse_misc_opts(int opt, const char *arg, const char *prog,
                    cli_options_t *opts)
{
    switch (opt) {
    case 'h':
        return handle_help(arg, prog, opts);
    case 'v':
        return handle_version(arg, prog, opts);
    case 'D':
    case CLI_OPT_DEFINE:
        return add_define_opt(arg, prog, opts);
    case 'U':
    case CLI_OPT_UNDEFINE:
        return add_undef_opt(arg, prog, opts);
    case 'c':
        opts->compile = true;
        return 0;
    case CLI_OPT_LINK:
        opts->link = true;
        return 0;
    case CLI_OPT_X86_64:
        opts->use_x86_64 = true;
        return 0;
    case CLI_OPT_INTEL_SYNTAX:
        opts->asm_syntax = ASM_INTEL;
        return 0;
    case 'S':
    case CLI_OPT_DUMP_ASM_LONG:
        opts->dump_asm = true;
        return 0;
    case CLI_OPT_DUMP_AST:
        opts->dump_ast = true;
        return 0;
    case CLI_OPT_DUMP_IR:
        opts->dump_ir = true;
        return 0;
    case CLI_OPT_DUMP_TOKENS:
        opts->dump_tokens = true;
        return 0;
    case CLI_OPT_DEBUG:
        opts->debug = true;
        return 0;
    case 'E':
        opts->preprocess = true;
        return 0;
    case CLI_OPT_NO_COLOR:
        opts->color_diag = false;
        return 0;
    case CLI_OPT_DEP_ONLY:
        opts->dep_only = true;
        return 0;
    case CLI_OPT_DEP:
        opts->deps = true;
        return 0;
    case CLI_OPT_NO_WARN_UNREACHABLE:
        opts->warn_unreachable = false;
        return 0;
    case CLI_OPT_EMIT_DWARF:
        opts->emit_dwarf = true;
        return 0;
    case CLI_OPT_STD:
        return handle_std(arg, prog, opts);
    default:
        return -1;
    }
}


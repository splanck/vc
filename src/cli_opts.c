#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "cli_opts.h"
#define VERSION "0.1.0"
#include "util.h"

/* usage message shared by multiple handlers */
void print_usage(const char *prog)
{
    printf("Usage: %s [options] <source...>\n", prog);
    printf("Options:\n");
    printf("  -o, --output <file>  Output path\n");
    printf("  -O<N>               Optimization level (0-3)\n");
    printf("  -I, --include <dir> Add directory to include search path\n");
    printf("  -L<dir>             Add library search path\n");
    printf("  -l<name>            Link against library\n");
    printf("  -Dname[=val]       Define a macro\n");
    printf("  -Uname             Undefine a macro\n");
    printf("      --std=<std>      Language standard (c99 or gnu99)\n");
    printf("  -h, --help           Display this help and exit\n");
    printf("  -v, --version        Print version information and exit\n");
    printf("  -c, --compile        Assemble to an object file\n");
    printf("      --link           Compile and link to an executable\n");
    printf("      --obj-dir <dir>  Directory for temporary object files\n");
    printf("      --no-fold        Disable constant folding\n");
    printf("      --no-dce         Disable dead code elimination\n");
    printf("      --no-cprop       Disable constant propagation\n");
    printf("      --no-inline      Disable inline expansion\n");
    printf("      --debug          Emit .file/.loc directives\n");
    printf("      --no-color       Disable colored diagnostics\n");
    printf("      --no-warn-unreachable  Disable unreachable code warnings\n");
    printf("      --x86-64         Generate 64-bit x86 assembly\n");
    printf("      --intel-syntax    Use Intel assembly syntax\n");
    printf("      --emit-dwarf      Include DWARF information\n");
    printf("  -S, --dump-asm       Print assembly to stdout and exit\n");
    printf("      --dump-ir        Print IR to stdout and exit\n");
    printf("      --dump-tokens    Print tokens to stdout and exit\n");
    printf("  -M                   Generate dependency file and exit\n");
    printf("  -MD                  Generate dependency file during compilation\n");
    printf("  -E, --preprocess     Run only the preprocessor and print the result\n");
    printf("  Provide '-' as a source file to read from standard input.\n");
}

static int push_source(cli_options_t *opts, const char *src)
{
    if (!vector_push(&opts->sources, &src)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int add_include_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->include_dirs, &dir)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int add_lib_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->lib_dirs, &dir)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int add_library(cli_options_t *opts, const char *name)
{
    if (!vector_push(&opts->libs, &name)) {
        vc_oom();
        return -1;
    }
    return 0;
}

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

static int handle_help(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)opts;
    print_usage(prog);
    exit(0);
}

static int handle_version(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog; (void)opts;
    printf("vc version %s\n", VERSION);
    exit(0);
}

static int add_define_opt(const char *arg, const char *prog, cli_options_t *opts)
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

static int add_undef_opt(const char *arg, const char *prog, cli_options_t *opts)
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

int finalize_options(int argc, char **argv, const char *prog,
                     cli_options_t *opts)
{
    if (optind >= argc) {
        fprintf(stderr, "Error: no source file specified.\n");
        print_usage(prog);
        cli_free_opts(opts);
        return 1;
    }

    for (int i = optind; i < argc; i++) {
        if (push_source(opts, argv[i])) {
            cli_free_opts(opts);
            return 1;
        }
    }

    if (!opts->output && !opts->dump_asm && !opts->dump_ir &&
        !opts->dump_tokens && !opts->dump_ast && !opts->preprocess &&
        !opts->dep_only) {
        fprintf(stderr, "Error: no output path specified.\n");
        print_usage(prog);
        cli_free_opts(opts);
        return 1;
    }

    return 0;
}


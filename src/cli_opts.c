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
    static const char *usage_lines[] = {
        "Usage: %s [options] <source...>\n",
        "Options:\n",
        "  -o, --output <file>  Output path\n",
        "  -O<N>               Optimization level (0-3)\n",
        "  -I, --include <dir> Add directory to include search path\n",
        "  -L<dir>             Add library search path\n",
        "  -l<name>            Link against library\n",
        "  -Dname[=val]       Define a macro\n",
        "  -Uname             Undefine a macro\n",
        "  -fmax-include-depth=<n>  Set maximum include depth\n",
        "      --std=<std>      Language standard (c99 or gnu99)\n",
        "  -h, --help           Display this help and exit\n",
        "  -v, --version        Print version information and exit\n",
        "  -c, --compile        Assemble to an object file\n",
        "      --link           Compile and link to an executable\n",
        "      --obj-dir <dir>  Directory for temporary object files\n",
        "      --sysroot <dir>  Prefix system include paths with <dir>\n",
        "      --no-fold        Disable constant folding\n",
        "      --no-dce         Disable dead code elimination\n",
        "      --no-cprop       Disable constant propagation\n",
        "      --no-inline      Disable inline expansion\n",
        "      --debug          Emit .file/.loc directives\n",
        "      --no-color       Disable colored diagnostics\n",
        "      --no-warn-unreachable  Disable unreachable code warnings\n",
        "      --x86-64         Generate 64-bit x86 assembly\n",
        "      --intel-syntax    Use Intel assembly syntax\n",
        "      --emit-dwarf      Include DWARF information\n",
        "  -S, --dump-asm       Print assembly to stdout and exit\n",
        "      --dump-ir        Print IR to stdout and exit\n",
        "      --dump-tokens    Print tokens to stdout and exit\n",
        "  -M                   Generate dependency file and exit\n",
        "  -MD                  Generate dependency file during compilation\n",
        "  -E, --preprocess     Run only the preprocessor and print the result\n",
        "  Provide '-' as a source file to read from standard input.\n",
    };

    for (size_t i = 0; i < sizeof(usage_lines) / sizeof(usage_lines[0]); i++) {
        if (i == 0)
            printf(usage_lines[i], prog);
        else
            printf("%s", usage_lines[i]);
    }
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

static int set_max_depth(cli_options_t *opts, const char *val)
{
    errno = 0;
    char *end;
    long long v = strtoll(val, &end, 10);
    if (*end != '\0' || errno != 0 || v <= 0 || v > INT_MAX) {
        fprintf(stderr, "Invalid include depth '%s'\n", val);
        return 1;
    }
    opts->max_include_depth = (size_t)v;
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
    case CLI_OPT_SYSROOT:
        opts->sysroot = (char *)arg;
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
    case 'f':
        if (strncmp(arg, "max-include-depth=", 18) == 0)
            return set_max_depth(opts, arg + 18);
        fprintf(stderr, "Unknown -f option '%s'\n", arg);
        return 1;
    case CLI_OPT_STD:
        return handle_std(arg, prog, opts);
    case CLI_OPT_FMAX_DEPTH:
        return set_max_depth(opts, arg);
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


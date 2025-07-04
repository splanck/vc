/*
 * Command line option parsing for the vc compiler.
 *
 * This module translates argv into a cli_options_t structure
 * that drives later compilation stages.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include "cli.h"

#define VERSION "0.1.0"

/*
 * Display a short usage message describing all supported
 * command line options.
 */
static void print_usage(const char *prog)
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
    printf("      --x86-64         Generate 64-bit x86 assembly\n");
    printf("      --intel-syntax    Use Intel assembly syntax\n");
    printf("  -S, --dump-asm       Print assembly to stdout and exit\n");
    printf("      --dump-ir        Print IR to stdout and exit\n");
    printf("  -E, --preprocess     Run only the preprocessor and print the result\n");
    printf("  Provide '-' as a source file to read from standard input.\n");
}

/*
 * Initialise "opts" with default settings.  Vectors are prepared for use
 * and all flags are reset to their default state.
 */
static void init_default_opts(cli_options_t *opts)
{
    if (!opts)
        return;

    opts->output = NULL;
    opts->opt_cfg.opt_level = 1;
    opts->opt_cfg.fold_constants = 1;
    opts->opt_cfg.dead_code = 1;
    opts->opt_cfg.const_prop = 1;
    opts->opt_cfg.inline_funcs = 1;
    opts->use_x86_64 = false;
    opts->compile = false;
    opts->link = false;
    opts->dump_asm = false;
    opts->dump_ir = false;
    opts->preprocess = false;
    opts->debug = false;
    opts->color_diag = true;
    opts->asm_syntax = ASM_ATT;
    opts->std = STD_C99;
    opts->obj_dir = "/tmp";
    vector_init(&opts->include_dirs, sizeof(char *));
    vector_init(&opts->sources, sizeof(char *));
    vector_init(&opts->defines, sizeof(char *));
    vector_init(&opts->undefines, sizeof(char *));
    vector_init(&opts->lib_dirs, sizeof(char *));
    vector_init(&opts->libs, sizeof(char *));
}

/*
 * Free all dynamic option vectors within "opts".
 */
void cli_free_opts(cli_options_t *opts)
{
    if (!opts)
        return;
    vector_free(&opts->sources);
    vector_free(&opts->include_dirs);
    vector_free(&opts->defines);
    vector_free(&opts->undefines);
    vector_free(&opts->lib_dirs);
    vector_free(&opts->libs);
}

/*
 * Append a source file path to "opts->sources".  Returns 0 on success and
 * non-zero on failure.
 */
static int push_source(cli_options_t *opts, const char *src)
{
    if (!vector_push(&opts->sources, &src)) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    return 0;
}

/*
 * Add an include directory to "opts->include_dirs". Returns 0 on success
 * and non-zero on failure.
 */
static int add_include_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->include_dirs, &dir)) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    return 0;
}

/*
 * Add a library search directory to "opts->lib_dirs".
 */
static int add_lib_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->lib_dirs, &dir)) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    return 0;
}

/*
 * Add a library name to "opts->libs".
 */
static int add_library(cli_options_t *opts, const char *name)
{
    if (!vector_push(&opts->libs, &name)) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    return 0;
}

/*
 * Set the optimization level and toggle individual passes accordingly.
 */
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

/*
 * Parse and set the language standard string. Returns 0 on success and 1
 * on unknown standard.
 */
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

/*
 * Individual option handlers used by handle_option().  Each returns 0 on
 * success or 1 on error.  Some handlers terminate the process directly when
 * the option requests help or version information.
 */
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

static int set_output_path(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    opts->output = (char *)arg;
    return 0;
}

static int enable_compile(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->compile = true;
    return 0;
}

static int add_include(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    return add_include_dir(opts, arg);
}

static int set_level(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    return set_opt_level(opts, arg);
}

static int disable_fold(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->opt_cfg.fold_constants = 0;
    return 0;
}

static int disable_dce(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->opt_cfg.dead_code = 0;
    return 0;
}

static int enable_x86(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->use_x86_64 = true;
    return 0;
}

static int enable_intel_syntax(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->asm_syntax = ASM_INTEL;
    return 0;
}

static int enable_dump(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->dump_asm = true;
    return 0;
}

static int disable_cprop(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->opt_cfg.const_prop = 0;
    return 0;
}

static int disable_inline_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->opt_cfg.inline_funcs = 0;
    return 0;
}

static int enable_dump_ir_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->dump_ir = true;
    return 0;
}

static int enable_debug_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->debug = true;
    return 0;
}

static int disable_color_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->color_diag = false;
    return 0;
}

static int enable_preproc(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->preprocess = true;
    return 0;
}

static int add_define_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -D option\n");
        return 1;
    }
    if (!vector_push(&opts->defines, &arg)) {
        fprintf(stderr, "Out of memory\n");
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
        fprintf(stderr, "Out of memory\n");
        return 1;
    }
    return 0;
}

static int add_lib_dir_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -L option\n");
        return 1;
    }
    return add_lib_dir(opts, arg);
}

static int add_lib_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -l option\n");
        return 1;
    }
    return add_library(opts, arg);
}

static int enable_link_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog;
    opts->link = true;
    return 0;
}

static int set_obj_dir_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    opts->obj_dir = (char *)arg;
    return 0;
}

static int handle_std(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    return set_standard(opts, arg);
}

/*
 * Handle a single getopt option by dispatching to the appropriate helper
 * function from a small lookup table. "opt" is the value returned by
 * getopt_long(), "arg" is the option argument if any and "prog" is used for
 * help output. Returns 0 on success and 1 on error. The process may exit for
 * -h/--help and -v/--version.
 */
static int handle_option(int opt, const char *arg, const char *prog,
                         cli_options_t *opts)
{
    struct opt_handler { int opt; int (*func)(const char *, const char *, cli_options_t *); };
    static const struct opt_handler table[] = {
        {'h', handle_help},
        {'v', handle_version},
        {'o', set_output_path},
        {'c', enable_compile},
        {'D', add_define_opt},
        {'U', add_undef_opt},
        {'I', add_include},
        {'L', add_lib_dir_opt},
        {'l', add_lib_opt},
        {'O', set_level},
        {CLI_OPT_NO_FOLD,      disable_fold},
        {CLI_OPT_NO_DCE,       disable_dce},
        {CLI_OPT_X86_64,       enable_x86},
        {CLI_OPT_INTEL_SYNTAX, enable_intel_syntax},
        {'S', enable_dump},
        {CLI_OPT_DUMP_ASM_LONG, enable_dump},
        {CLI_OPT_NO_CPROP,     disable_cprop},
        {CLI_OPT_DUMP_IR,      enable_dump_ir_opt},
        {CLI_OPT_DEBUG,        enable_debug_opt},
        {CLI_OPT_NO_INLINE,    disable_inline_opt},
        {CLI_OPT_NO_COLOR,     disable_color_opt},
        {'E', enable_preproc},
        {CLI_OPT_DEFINE,       add_define_opt},
        {CLI_OPT_UNDEFINE,     add_undef_opt},
        {CLI_OPT_LINK,         enable_link_opt},
        {CLI_OPT_STD,          handle_std},
        {CLI_OPT_OBJ_DIR,      set_obj_dir_opt},
        {0,   NULL}
    };

    for (size_t i = 0; table[i].func; i++) {
        if (table[i].opt == opt)
            return table[i].func(arg, prog, opts);
    }

    print_usage(prog);
    return 1;
}

/*
 * Parse argv using getopt_long and fill the cli_options_t structure
 * with the selected settings. The long_opts table defines the
 * mapping between short (-o) and long (--output) options. Default
 * values are initialized before parsing. Returns 0 on success and a
 * non-zero value on failure.
 */
int cli_parse_args(int argc, char **argv, cli_options_t *opts)
{
    /* getopt_long maintains state between calls, reset for reuse */
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif

    static struct option long_opts[] = {
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {"output",  required_argument, 0, 'o'},
        {"include", required_argument, 0, 'I'},
        {"compile", no_argument,       0, 'c'},
        {"no-fold", no_argument,       0, CLI_OPT_NO_FOLD},
        {"no-dce",  no_argument,       0, CLI_OPT_NO_DCE},
        {"x86-64",  no_argument,       0, CLI_OPT_X86_64},
        {"intel-syntax", no_argument, 0, CLI_OPT_INTEL_SYNTAX},
        {"dump-asm", no_argument,     0, CLI_OPT_DUMP_ASM_LONG},
        {"no-cprop", no_argument,     0, CLI_OPT_NO_CPROP},
        {"no-inline", no_argument,   0, CLI_OPT_NO_INLINE},
        {"dump-ir", no_argument,      0, CLI_OPT_DUMP_IR},
        {"debug", no_argument,       0, CLI_OPT_DEBUG},
        {"define", required_argument, 0, CLI_OPT_DEFINE},
        {"undefine", required_argument, 0, CLI_OPT_UNDEFINE},
        {"preprocess", no_argument,  0, 'E'},
        {"link", no_argument,        0, CLI_OPT_LINK},
        {"std", required_argument,   0, CLI_OPT_STD},
        {"obj-dir", required_argument, 0, CLI_OPT_OBJ_DIR},
        {"no-color", no_argument, 0, CLI_OPT_NO_COLOR},
        {0, 0, 0, 0}
    };

    init_default_opts(opts);

    int opt;
    while ((opt = getopt_long(argc, argv, "hvo:O:cD:U:I:L:l:ES", long_opts, NULL)) != -1) {
        if (handle_option(opt, optarg, argv[0], opts)) {
            cli_free_opts(opts);
            return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: no source file specified.\n");
        print_usage(argv[0]);
        cli_free_opts(opts);
        return 1;
    }

    for (int i = optind; i < argc; i++) {
        if (push_source(opts, argv[i])) {
            cli_free_opts(opts);
            return 1;
        }
    }

    if (!opts->output && !opts->dump_asm && !opts->dump_ir && !opts->preprocess) {
        fprintf(stderr, "Error: no output path specified.\n");
        print_usage(argv[0]);
        cli_free_opts(opts);
        return 1;
    }

    return 0;
}


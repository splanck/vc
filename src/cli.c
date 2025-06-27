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
    printf("      --std=<std>      Language standard (c99 or gnu99)\n");
    printf("  -h, --help           Display this help and exit\n");
    printf("  -v, --version        Print version information and exit\n");
    printf("  -c, --compile        Assemble to an object file\n");
    printf("      --link           Compile and link to an executable\n");
    printf("      --no-fold        Disable constant folding\n");
    printf("      --no-dce         Disable dead code elimination\n");
    printf("      --no-cprop       Disable constant propagation\n");
    printf("      --x86-64         Generate 64-bit x86 assembly\n");
    printf("  -S, --dump-asm       Print assembly to stdout and exit\n");
    printf("      --dump-ir        Print IR to stdout and exit\n");
    printf("  -E, --preprocess     Run only the preprocessor and print the result\n");
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
    opts->use_x86_64 = 0;
    opts->compile = 0;
    opts->link = 0;
    opts->dump_asm = 0;
    opts->dump_ir = 0;
    opts->preprocess = 0;
    opts->std = STD_C99;
    vector_init(&opts->include_dirs, sizeof(char *));
    vector_init(&opts->sources, sizeof(char *));
}

/*
 * Append a source file path to "opts->sources".  Returns 0 on success and
 * 1 on out-of-memory failure.
 */
static int push_source(cli_options_t *opts, const char *src)
{
    if (!vector_push(&opts->sources, &src)) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }
    return 0;
}

/*
 * Add an include directory to "opts->include_dirs". Returns 0 on success
 * and 1 on out-of-memory failure.
 */
static int add_include_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->include_dirs, &dir)) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }
    return 0;
}

/*
 * Set the optimization level and toggle individual passes accordingly.
 */
static void set_opt_level(cli_options_t *opts, const char *level)
{
    opts->opt_cfg.opt_level = atoi(level);
    if (opts->opt_cfg.opt_level <= 0) {
        opts->opt_cfg.fold_constants = 0;
        opts->opt_cfg.dead_code = 0;
        opts->opt_cfg.const_prop = 0;
    } else {
        opts->opt_cfg.fold_constants = 1;
        opts->opt_cfg.dead_code = 1;
        opts->opt_cfg.const_prop = 1;
    }
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
 * Handle a single getopt option.  "opt" is the value returned by
 * getopt_long(), "arg" is the option argument if any and "prog" is used for
 * help output.  Returns 0 on success and 1 on error.  The function may exit
 * the process for -h/--help and -v/--version.
 */
static int handle_option(int opt, const char *arg, const char *prog,
                         cli_options_t *opts)
{
    switch (opt) {
    case 'h':
        print_usage(prog);
        exit(0);
    case 'v':
        printf("vc version %s\n", VERSION);
        exit(0);
    case 'o':
        opts->output = (char *)arg;
        break;
    case 'c':
        opts->compile = 1;
        break;
    case 'I':
        if (add_include_dir(opts, arg))
            return 1;
        break;
    case 'O':
        set_opt_level(opts, arg);
        break;
    case 1:
        opts->opt_cfg.fold_constants = 0;
        break;
    case 2:
        opts->opt_cfg.dead_code = 0;
        break;
    case 3:
        opts->use_x86_64 = 1;
        break;
    case 'S':
    case 4:
        opts->dump_asm = 1;
        break;
    case 5:
        opts->opt_cfg.const_prop = 0;
        break;
    case 6:
        opts->dump_ir = 1;
        break;
    case 'E':
        opts->preprocess = 1;
        break;
    case 7:
        opts->link = 1;
        break;
    case 8:
        if (set_standard(opts, arg))
            return 1;
        break;
    default:
        print_usage(prog);
        return 1;
    }

    return 0;
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
    static struct option long_opts[] = {
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {"output",  required_argument, 0, 'o'},
        {"include", required_argument, 0, 'I'},
        {"compile", no_argument,       0, 'c'},
        {"no-fold", no_argument,       0, 1},
        {"no-dce",  no_argument,       0, 2},
        {"x86-64", no_argument,       0, 3},
        {"dump-asm", no_argument,     0, 4},
        {"no-cprop", no_argument,     0, 5},
        {"dump-ir", no_argument,      0, 6},
        {"preprocess", no_argument,  0, 'E'},
        {"link", no_argument,        0, 7},
        {"std", required_argument,   0, 8},
        {0, 0, 0, 0}
    };

    init_default_opts(opts);

    int opt;
    while ((opt = getopt_long(argc, argv, "hvo:O:cEI:S", long_opts, NULL)) != -1) {
        if (handle_option(opt, optarg, argv[0], opts))
            return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: no source file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (!opts->output && !opts->dump_asm && !opts->dump_ir && !opts->preprocess) {
        fprintf(stderr, "Error: no output path specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    for (int i = optind; i < argc; i++) {
        if (push_source(opts, argv[i]))
            return 1;
    }

    return 0;
}


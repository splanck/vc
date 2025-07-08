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
#include "cli_options.h"
#include "util.h"

#define VERSION "0.1.0"

/*
 * Display a short usage message describing all supported
 * command line options.
 */
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
    opts->dump_ast = false;
    opts->dump_ir = false;
    opts->dump_tokens = false;
    opts->preprocess = false;
    opts->debug = false;
    opts->emit_dwarf = false;
    opts->color_diag = true;
    opts->dep_only = false;
    opts->deps = false;
    opts->warn_unreachable = true;
    opts->asm_syntax = ASM_ATT;
    opts->std = STD_C99;
    opts->obj_dir = NULL;
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
        vc_oom();
        return -1;
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
    /* getopt_long maintains state between calls, reset for reuse */
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif

    /* Prepend flags from VCFLAGS environment variable */
    char **vcflags_argv = NULL;
    char *vcflags_buf = NULL;
    int vcflags_argc = 0;
    const char *env = getenv("VCFLAGS");
    if (env && *env) {
        vcflags_buf = vc_strdup(env);
        if (!vcflags_buf) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            return 1;
        }

        /* Count tokens */
        char *tmp = vc_strdup(env);
        if (!tmp) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            return 1;
        }
        for (char *t = strtok(tmp, " "); t; t = strtok(NULL, " "))
            vcflags_argc++;
        free(tmp);

        vcflags_argv = malloc(sizeof(char *) * (argc + vcflags_argc));
        if (!vcflags_argv) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            return 1;
        }

        vcflags_argv[0] = argv[0];
        int idx = 1;
        for (char *t = strtok(vcflags_buf, " "); t; t = strtok(NULL, " "))
            vcflags_argv[idx++] = t;
        for (int i = 1; i < argc; i++)
            vcflags_argv[idx++] = argv[i];

        argv = vcflags_argv;
        argc += vcflags_argc;
    }

    /* Pre-scan argv for -M and -MD options */
    int new_argc = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-MD") == 0) {
            argv[new_argc++] = "--MD";
        } else if (strcmp(argv[i], "-M") == 0) {
            argv[new_argc++] = "--M";
        } else {
            argv[new_argc++] = argv[i];
        }
    }
    argc = new_argc;

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
        {"dump-ast", no_argument,     0, CLI_OPT_DUMP_AST},
        {"no-cprop", no_argument,     0, CLI_OPT_NO_CPROP},
        {"no-inline", no_argument,   0, CLI_OPT_NO_INLINE},
        {"dump-ir", no_argument,      0, CLI_OPT_DUMP_IR},
        {"dump-tokens", no_argument, 0, CLI_OPT_DUMP_TOKENS},
        {"debug", no_argument,       0, CLI_OPT_DEBUG},
        {"define", required_argument, 0, CLI_OPT_DEFINE},
        {"undefine", required_argument, 0, CLI_OPT_UNDEFINE},
        {"preprocess", no_argument,  0, 'E'},
        {"link", no_argument,        0, CLI_OPT_LINK},
        {"MD", no_argument,         0, CLI_OPT_DEP},
        {"M", no_argument,          0, CLI_OPT_DEP_ONLY},
        {"std", required_argument,   0, CLI_OPT_STD},
        {"obj-dir", required_argument, 0, CLI_OPT_OBJ_DIR},
        {"no-color", no_argument, 0, CLI_OPT_NO_COLOR},
        {"no-warn-unreachable", no_argument, 0, CLI_OPT_NO_WARN_UNREACHABLE},
        {"emit-dwarf", no_argument, 0, CLI_OPT_EMIT_DWARF},
        {0, 0, 0, 0}
    };

    init_default_opts(opts);

    int opt;
    while ((opt = getopt_long(argc, argv, "hvo:O:cD:U:I:L:l:ES", long_opts, NULL)) != -1) {
        int ret;
        if ((ret = parse_optimization_opts(opt, optarg, opts)) == 1) {
            cli_free_opts(opts);
            return 1;
        } else if (ret == 0) {
            continue;
        }

        if ((ret = parse_io_paths(opt, optarg, opts)) == 1) {
            cli_free_opts(opts);
            return 1;
        } else if (ret == 0) {
            continue;
        }

        if ((ret = parse_misc_opts(opt, optarg, argv[0], opts)) == 1) {
            cli_free_opts(opts);
            return 1;
        } else if (ret == 0) {
            continue;
        }

    print_usage(argv[0]);
    cli_free_opts(opts);
    return 1;
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

    if (!opts->output && !opts->dump_asm && !opts->dump_ir &&
        !opts->dump_tokens && !opts->dump_ast && !opts->preprocess &&
        !opts->dep_only) {
        fprintf(stderr, "Error: no output path specified.\n");
        print_usage(argv[0]);
        cli_free_opts(opts);
        return 1;
    }

    return 0;
}


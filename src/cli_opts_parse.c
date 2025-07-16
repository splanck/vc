/* Command line option parsing for the vc compiler.
 * Part of vc under the BSD 2-Clause license. See LICENSE for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include "cli.h"
#include "util.h"
#include "cli_opts_env.h"
#include "cli_opts.h"
#include "preproc_file.h"

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
    opts->sysroot = NULL;
    opts->vc_sysinclude = NULL;
    opts->internal_libc = false;
    opts->verbose_includes = false;
    opts->named_locals = false;
    opts->max_include_depth = DEFAULT_INCLUDE_DEPTH;
    vector_init(&opts->include_dirs, sizeof(char *));
    vector_init(&opts->sources, sizeof(char *));
    vector_init(&opts->defines, sizeof(char *));
    vector_init(&opts->undefines, sizeof(char *));
    vector_init(&opts->lib_dirs, sizeof(char *));
    vector_init(&opts->libs, sizeof(char *));
}

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
 * Free option structures and any resources allocated for VCFLAGS
 * processing, then signal an error.
 */
static int cleanup_parse_error(cli_options_t *opts, char **vcflags_argv,
                               char *vcflags_buf)
{
    cli_free_opts(opts);
    free(vcflags_argv);
    free(vcflags_buf);
    return 1;
}

static int push_source(cli_options_t *opts, const char *src)
{
    if (!vector_push(&opts->sources, &src)) {
        vc_oom();
        return -1;
    }
    return 0;
}

int cli_parse_args(int argc, char **argv, cli_options_t *opts)
{
    optind = 1;
#ifdef __BSD_VISIBLE
    optreset = 1;
#endif

    char **vcflags_argv = NULL;
    char *vcflags_buf = NULL;

    if (load_vcflags(&argc, &argv, &vcflags_argv, &vcflags_buf))
        return 1;

    scan_shortcuts(&argc, argv);

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
        {"fmax-include-depth", required_argument, 0, CLI_OPT_FMAX_DEPTH},
        {"sysroot", required_argument, 0, CLI_OPT_SYSROOT},
        {"vc-sysinclude", required_argument, 0, CLI_OPT_VC_SYSINCLUDE},
        {"internal-libc", no_argument, 0, CLI_OPT_INTERNAL_LIBC},
        {"verbose-includes", no_argument, 0, CLI_OPT_VERBOSE_INCLUDES},
        {"named-locals", no_argument, 0, CLI_OPT_NAMED_LOCALS},
        {0, 0, 0, 0}
    };

    init_default_opts(opts);

    int opt;
    while ((opt = getopt_long(argc, argv, "hvo:O:cD:U:I:L:l:ESf:", long_opts, NULL)) != -1) {
        int ret;
        if ((ret = parse_optimization_opts(opt, optarg, opts)) == 1) {
            return cleanup_parse_error(opts, vcflags_argv, vcflags_buf);
        } else if (ret == 0) {
            continue;
        }

        if ((ret = parse_io_paths(opt, optarg, opts)) == 1) {
            return cleanup_parse_error(opts, vcflags_argv, vcflags_buf);
        } else if (ret == 0) {
            continue;
        }

        if ((ret = parse_misc_opts(opt, optarg, argv[0], opts)) == 1) {
            return cleanup_parse_error(opts, vcflags_argv, vcflags_buf);
        } else if (ret == 0) {
            continue;
        }

        print_usage(argv[0]);
        return cleanup_parse_error(opts, vcflags_argv, vcflags_buf);
    }

    int ret = finalize_options(argc, argv, argv[0], opts);
    free(vcflags_argv);
    free(vcflags_buf);
    return ret;
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

    if (opts->internal_libc) {
        if (cli_setup_internal_libc(opts, prog)) {
            cli_free_opts(opts);
            return 1;
        }
    }

    return 0;
}


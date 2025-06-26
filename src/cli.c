/*
 * Command line option parsing.
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
    printf("      --dump-asm       Print assembly to stdout and exit\n");
    printf("      --dump-ir        Print IR to stdout and exit\n");
    printf("  -E, --preprocess     Print preprocessed source and exit\n");
}

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

    int opt;
    while ((opt = getopt_long(argc, argv, "hvo:O:cEI:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            exit(0);
        case 'v':
            printf("vc version %s\n", VERSION);
            exit(0);
        case 'o':
            opts->output = optarg;
            break;
        case 'c':
            opts->compile = 1;
            break;
        case 'I':
            if (!vector_push(&opts->include_dirs, &optarg)) {
                fprintf(stderr, "Out of memory\n");
                return 1;
            }
            break;
        case 'O':
            opts->opt_cfg.opt_level = atoi(optarg);
            if (opts->opt_cfg.opt_level <= 0) {
                opts->opt_cfg.fold_constants = 0;
                opts->opt_cfg.dead_code = 0;
                opts->opt_cfg.const_prop = 0;
            } else {
                opts->opt_cfg.fold_constants = 1;
                opts->opt_cfg.dead_code = 1;
                opts->opt_cfg.const_prop = 1;
            }
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
            if (strcmp(optarg, "c99") == 0)
                opts->std = STD_C99;
            else if (strcmp(optarg, "gnu99") == 0)
                opts->std = STD_GNU99;
            else {
                fprintf(stderr, "Unknown standard '%s'\n", optarg);
                return 1;
            }
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
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
        if (!vector_push(&opts->sources, &argv[i])) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
    }

    return 0;
}


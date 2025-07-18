#define _POSIX_C_SOURCE 200809L
/*
 * Entry point of the vc compiler.
 *
 * Command line arguments are parsed and the compilation
 * pipeline is dispatched via helper functions in compile.c.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cli.h"
#include "compile.h"
#include "error.h"
#include "semantic_stmt.h"

/*
 * Program entry point. Parses command line options and coordinates
 * preprocessing, compilation and linking.
 */
int main(int argc, char **argv)
{
    cli_options_t cli;
    int ret = 1;

    if (cli_parse_args(argc, argv, &cli) != 0)
        goto cleanup;

    error_use_color = cli.color_diag;
    semantic_warn_unreachable = cli.warn_unreachable;
    semantic_suppress_warnings = false;


    if (cli.preprocess) {
        ret = run_preprocessor(&cli);
        goto cleanup;
    }

    if (cli.dep_only) {
        ret = generate_dependencies(&cli);
        goto cleanup;
    }

    int ok = 1;
    if (cli.link) {
        ok = link_sources(&cli);
    } else {
        const char *src = ((const char **)cli.sources.data)[0];
        ok = compile_unit(src, &cli, cli.output, cli.compile);
    }

    if (ok) {
        if (cli.link)
            printf("Linking %zu files -> %s (executable)\n", cli.sources.count,
                   cli.output);
        else if (cli.dump_ir)
            printf("Compiling %s (IR dumped to stdout)\n",
                   ((const char **)cli.sources.data)[0]);
        else if (cli.dump_ast)
            printf("Compiling %s (AST dumped to stdout)\n",
                   ((const char **)cli.sources.data)[0]);
        else if (cli.dump_asm)
            printf("Compiling %s (assembly dumped to stdout)\n",
                   ((const char **)cli.sources.data)[0]);
        else if (cli.compile && cli.sources.count > 1)
            printf("Compiled %zu files to objects\n", cli.sources.count);
        else if (cli.compile)
            printf("Compiling %s -> %s (object)\n",
                   ((const char **)cli.sources.data)[0], cli.output);
        else
            printf("Compiling %s -> %s\n",
                   ((const char **)cli.sources.data)[0], cli.output);
    }

    ret = ok ? 0 : 1;

cleanup:
    cli_free_opts(&cli);
    return ret;
}

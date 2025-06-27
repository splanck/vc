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

/*
 * Program entry point. Parses command line options and coordinates
 * preprocessing, compilation and linking.
 */
int main(int argc, char **argv)
{
    cli_options_t cli;
    if (cli_parse_args(argc, argv, &cli) != 0)
        return 1;

    if (!cli.link && cli.sources.count != 1) {
        fprintf(stderr, "Error: multiple input files require --link\n");
        return 1;
    }

    if (cli.preprocess)
        return run_preprocessor(&cli);

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
        else if (cli.dump_asm)
            printf("Compiling %s (assembly dumped to stdout)\n",
                   ((const char **)cli.sources.data)[0]);
        else if (cli.compile)
            printf("Compiling %s -> %s (object)\n",
                   ((const char **)cli.sources.data)[0], cli.output);
        else
            printf("Compiling %s -> %s\n",
                   ((const char **)cli.sources.data)[0], cli.output);
    }

    return ok ? 0 : 1;
}

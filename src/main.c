#define _POSIX_C_SOURCE 200809L
/*
 * Entry point of the vc compiler.
 *
 * This file drives the entire compilation pipeline:
 *  1. Source code is tokenized by the lexer.
 *  2. The parser builds an AST from those tokens.
 *  3. Semantic analysis creates an intermediate representation (IR).
 *  4. Optimization passes run on the IR.
 *  5. Code generation emits x86 assembly or an object file.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"
#include "cli.h"
#include "vector.h"
#include <string.h>
#include "preproc.h"
#include "compile.h"

int main(int argc, char **argv)
{
    cli_options_t cli;
    if (cli_parse_args(argc, argv, &cli) != 0)
        return 1;

    if (!cli.link && cli.sources.count != 1) {
        fprintf(stderr, "Error: multiple input files require --link\n");
        return 1;
    }

    /* Only run the preprocessor when -E/--preprocess is supplied */
    if (cli.preprocess) {
        for (size_t i = 0; i < cli.sources.count; i++) {
            const char *src = ((const char **)cli.sources.data)[i];
            char *text = preproc_run(src, &cli.include_dirs);
            if (!text) {
                perror("preproc_run");
                return 1;
            }
            printf("%s", text);
            free(text);
        }
        return 0;
    }

    int ok = 1;

    if (cli.link) {
        vector_t objs;
        vector_init(&objs, sizeof(char *));
        for (size_t i = 0; i < cli.sources.count && ok; i++) {
            const char *src = ((const char **)cli.sources.data)[i];
            char objname[] = "/tmp/vcobjXXXXXX";
            int fd = mkstemp(objname);
            if (fd < 0) {
                perror("mkstemp");
                ok = 0;
                break;
            }
            close(fd);
            ok = compile_unit(src, &cli, objname, 1);
            if (ok) {
                char *dup = vc_strdup(objname);
                if (!vector_push(&objs, &dup)) {
                    fprintf(stderr, "Out of memory\n");
                    ok = 0;
                }
            }
            if (!ok)
                unlink(objname);
        }

        char *stubobj = NULL;
        if (ok)
            ok = create_startup_object(cli.use_x86_64, &stubobj);
        if (ok) {
            vector_push(&objs, &stubobj);
            size_t cmd_len = 64;
            for (size_t i = 0; i < objs.count; i++)
                cmd_len += strlen(((char **)objs.data)[i]) + 1;
            cmd_len += strlen(cli.output) + 1;
            char *cmd = vc_alloc_or_exit(cmd_len + 32);
            const char *arch_flag = cli.use_x86_64 ? "-m64" : "-m32";
            snprintf(cmd, cmd_len + 32, "cc %s", arch_flag);
            for (size_t i = 0; i < objs.count; i++) {
                strcat(cmd, " ");
                strcat(cmd, ((char **)objs.data)[i]);
            }
            strcat(cmd, " -nostdlib -o ");
            strcat(cmd, cli.output);
            int ret = system(cmd);
            if (ret != 0) {
                fprintf(stderr, "cc failed\n");
                ok = 0;
            }
            free(cmd);
        }

        for (size_t i = 0; i < objs.count; i++) {
            unlink(((char **)objs.data)[i]);
            free(((char **)objs.data)[i]);
        }
        free(objs.data);
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

#define _POSIX_C_SOURCE 200809L
/*
 * High level compilation and linking routines.
 *
 * This module contains helper functions used by main.c to run the
 * preprocessor, compile translation units and link executables.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#ifndef PATH_MAX
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

#include "util.h"
#include "cli.h"
#include "token.h"
#include "parser.h"
#include "parser_core.h"
#include "ast_stmt.h"
#include "vector.h"
#include "symtable.h"
#include "semantic.h"
#include "error.h"
#include "ir_core.h"
#include "ir_dump.h"
#include "ast_dump.h"
#include "opt.h"
#include "codegen.h"
#include "label.h"
#include "preproc.h"
#include "command.h"
#include "compile.h"
#include "compile_stage.h"
#include "startup.h"

/* Use binary mode for temporary files on platforms that require it */
#if defined(_WIN32)
# define TEMP_FOPEN_MODE "wb"
#else
# define TEMP_FOPEN_MODE "w"
#endif

/* Active diagnostic context */
extern const char *error_current_file;
extern const char *error_current_function;
extern char **environ;

char *vc_obj_name(const char *source);


/*
 * Assemble mkstemp template path using cli->obj_dir (or the process
 * temporary directory) and the given prefix.  Returns a newly allocated
 * string or NULL on error.
 *
 * errno will be ENAMETOOLONG if the resulting path would exceed PATH_MAX
 * or snprintf detected truncation.
 */
static char *
create_temp_template(const cli_options_t *cli, const char *prefix)
{
    const char *dir = cli->obj_dir;
    if (!dir || !*dir) {
        dir = getenv("TMPDIR");
        if (!dir || !*dir) {
#ifdef P_tmpdir
            dir = P_tmpdir;
#else
            dir = "/tmp";
#endif
        }
    }
    size_t len = strlen(dir) + strlen(prefix) + sizeof("/XXXXXX");
    if (len >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    char *tmpl = malloc(len + 1);
    if (!tmpl)
        return NULL;

    errno = 0;
    int n = snprintf(tmpl, len + 1, "%s/%sXXXXXX", dir, prefix);
    int err = errno;
    if (n < 0) {
        free(tmpl);
        errno = err;
        return NULL;
    }
    if ((size_t)n >= len + 1) {
        free(tmpl);
        errno = ENAMETOOLONG;
        return NULL;
    }

    return tmpl;
}

/*
 * Create and open the temporary file described by tmpl.  Returns the file
 * descriptor on success or -1 on failure.  On error the file is unlinked
 * and errno is preserved.
 */
static int
open_temp_file(char *tmpl)
{
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        int err = errno;
        close(fd);
        unlink(tmpl);
        errno = err;
        return -1;
    }
    return fd;
}

/*
 * Create a temporary file and return its descriptor.  On success the path
 * is stored in *out_path.  On failure -1 is returned, *out_path is set to
 * NULL and errno indicates the error:
 *   ENAMETOOLONG - path would exceed PATH_MAX or snprintf truncated
 *   others       - from malloc, mkstemp or fcntl
 */
int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path)
{
    char *tmpl = create_temp_template(cli, prefix);
    if (!tmpl) {
        *out_path = NULL;
        return -1;
    }

    int fd = open_temp_file(tmpl);
    if (fd < 0) {
        free(tmpl);
        *out_path = NULL;
        return -1;
    }

    *out_path = tmpl;
    return fd;
}

/* Run the preprocessor and print the result. */
int run_preprocessor(const cli_options_t *cli)
{
    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        preproc_context_t ctx;
        char *text = preproc_run(&ctx, src, &cli->include_dirs, &cli->defines,
                                &cli->undefines);
        if (!text) {
            perror("preproc_run");
            return 1;
        }
        size_t len = strlen(text);
        if (fwrite(text, 1, len, stdout) != len) {
            perror("fwrite");
            free(text);
            return 1;
        }
        if (len == 0 || text[len - 1] != '\n') {
            if (putchar('\n') == EOF) {
                perror("putchar");
                free(text);
                return 1;
            }
        }
        if (fflush(stdout) == EOF) {
            perror("fflush");
            free(text);
            return 1;
        }
        free(text);
    }
    return 0;
}

/* Generate dependency files without compiling */
int generate_dependencies(const cli_options_t *cli)
{
    for (size_t i = 0; i < cli->sources.count; i++) {
        const char *src = ((const char **)cli->sources.data)[i];
        preproc_context_t ctx;
        char *text = preproc_run(&ctx, src, &cli->include_dirs,
                                 &cli->defines, &cli->undefines);
        if (!text) {
            perror("preproc_run");
            return 1;
        }
        free(text);
        if (!write_dep_file(src, &ctx.deps)) {
            preproc_context_free(&ctx);
            return 1;
        }
        preproc_context_free(&ctx);
    }
    return 0;
}

#ifndef UNIT_TESTING
int compile_unit(const char *source, const cli_options_t *cli,
                 const char *output, int compile_obj)
{
    if (!cli->link && compile_obj && cli->sources.count > 1) {
        int ok = 1;
        for (size_t i = 0; i < cli->sources.count && ok; i++) {
            const char *src = ((const char **)cli->sources.data)[i];
            char *obj = vc_obj_name(src);
            if (!obj) {
                vc_oom();
                return 0;
            }
            ok = compile_pipeline(src, cli, obj, 1);
            free(obj);
        }
        return ok;
    }

    return compile_pipeline(source, cli, output, compile_obj);
}
#endif /* !UNIT_TESTING */




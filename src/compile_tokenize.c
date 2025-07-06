#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"
#include "cli.h"
#include "token.h"
#include "preproc.h"

/* Use binary mode for temporary files on platforms that require it */
#if defined(_WIN32)
# define TEMP_FOPEN_MODE "wb"
#else
# define TEMP_FOPEN_MODE "w"
#endif

int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);

static int read_stdin_source(const cli_options_t *cli,
                             const vector_t *incdirs,
                             const vector_t *defines,
                             const vector_t *undefines,
                             char **out_path, char **out_text)
{
    char *path = NULL;
    int fd = create_temp_file(cli, "vcstdin", &path);
    if (fd < 0) {
        perror("mkstemp");
        return 0;
    }
    FILE *f = fdopen(fd, TEMP_FOPEN_MODE);
    if (!f) {
        perror("fdopen");
        close(fd);
        unlink(path);
        free(path);
        return 0;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        if (fwrite(buf, 1, n, f) != n) {
            perror("fwrite");
            if (fclose(f) == EOF)
                perror("fclose");
            unlink(path);
            free(path);
            return 0;
        }
    }
    if (ferror(stdin)) {
        perror("fread");
        if (fclose(f) == EOF)
            perror("fclose");
        unlink(path);
        free(path);
        return 0;
    }
    if (fclose(f) == EOF) {
        perror("fclose");
        unlink(path);
        free(path);
        return 0;
    }

    preproc_context_t ctx;
    char *text = preproc_run(&ctx, path, incdirs, defines, undefines);
    if (!text) {
        perror("preproc_run");
        unlink(path);
        free(path);
        return 0;
    }

    *out_path = path;
    *out_text = text;
    return 1;
}

int compile_tokenize_impl(const char *source, const cli_options_t *cli,
                          const vector_t *incdirs, const vector_t *defines,
                          const vector_t *undefines, char **out_src,
                          token_t **out_toks, size_t *out_count,
                          char **tmp_path)
{
    if (tmp_path)
        *tmp_path = NULL;

    char *stdin_path = NULL;
    char *text = NULL;
    if (source && strcmp(source, "-") == 0) {
        if (!read_stdin_source(cli, incdirs, defines, undefines,
                               &stdin_path, &text))
            return 0;
        if (tmp_path)
            *tmp_path = stdin_path;
        else {
            unlink(stdin_path);
            free(stdin_path);
            stdin_path = NULL;
        }
    } else {
        preproc_context_t ctx;
        text = preproc_run(&ctx, source, incdirs, defines, undefines);
        if (!text) {
            perror("preproc_run");
            return 0;
        }
    }

    size_t count = 0;
    token_t *toks = lexer_tokenize(text, &count);
    if (!toks) {
        fprintf(stderr, "Tokenization failed\n");
        free(text);
        if (stdin_path) {
            unlink(stdin_path);
            free(stdin_path);
        }
        return 0;
    }

    *out_src = text;
    *out_toks = toks;
    if (out_count)
        *out_count = count;
    return 1;
}


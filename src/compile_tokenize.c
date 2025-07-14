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
#include "strbuf.h"
#include "semantic_global.h"
#include "semantic_stmt.h"

/* Use binary mode for temporary files on platforms that require it */
#if defined(_WIN32)
# define TEMP_FOPEN_MODE "wb"
#else
# define TEMP_FOPEN_MODE "w"
#endif

int create_temp_file(const cli_options_t *cli, const char *prefix,
                     char **out_path);

/* Table mapping token types to printable names */
static const char *tok_names[] = {
    [TOK_EOF] = "end of file",
    [TOK_IDENT] = "identifier",
    [TOK_NUMBER] = "number",
    [TOK_STRING] = "string",
    [TOK_CHAR] = "character",
    [TOK_WIDE_STRING] = "L\"string\"",
    [TOK_WIDE_CHAR] = "L'char'",
    [TOK_KW_INT] = "\"int\"",
    [TOK_KW_CHAR] = "\"char\"",
    [TOK_KW_FLOAT] = "\"float\"",
    [TOK_KW_DOUBLE] = "\"double\"",
    [TOK_KW_SHORT] = "\"short\"",
    [TOK_KW_LONG] = "\"long\"",
    [TOK_KW_BOOL] = "\"bool\"",
    [TOK_KW_UNSIGNED] = "\"unsigned\"",
    [TOK_KW_VOID] = "\"void\"",
    [TOK_KW_ENUM] = "\"enum\"",
    [TOK_KW_STRUCT] = "\"struct\"",
    [TOK_KW_UNION] = "\"union\"",
    [TOK_KW_TYPEDEF] = "\"typedef\"",
    [TOK_KW_STATIC] = "\"static\"",
    [TOK_KW_EXTERN] = "\"extern\"",
    [TOK_KW_CONST] = "\"const\"",
    [TOK_KW_VOLATILE] = "\"volatile\"",
    [TOK_KW_RESTRICT] = "\"restrict\"",
    [TOK_KW_REGISTER] = "\"register\"",
    [TOK_KW_INLINE] = "\"inline\"",
    [TOK_KW_NORETURN] = "\"_Noreturn\"",
    [TOK_KW_STATIC_ASSERT] = "\"_Static_assert\"",
    [TOK_KW_RETURN] = "\"return\"",
    [TOK_KW_IF] = "\"if\"",
    [TOK_KW_ELSE] = "\"else\"",
    [TOK_KW_DO] = "\"do\"",
    [TOK_KW_WHILE] = "\"while\"",
    [TOK_KW_FOR] = "\"for\"",
    [TOK_KW_BREAK] = "\"break\"",
    [TOK_KW_CONTINUE] = "\"continue\"",
    [TOK_KW_GOTO] = "\"goto\"",
    [TOK_KW_SWITCH] = "\"switch\"",
    [TOK_KW_CASE] = "\"case\"",
    [TOK_KW_DEFAULT] = "\"default\"",
    [TOK_KW_SIZEOF] = "\"sizeof\"",
    [TOK_KW_COMPLEX] = "\"_Complex\"",
    [TOK_KW_ALIGNAS] = "\"alignas\"",
    [TOK_KW_ALIGNOF] = "\"_Alignof\"",
    [TOK_LPAREN] = "'('",
    [TOK_RPAREN] = ")",
    [TOK_LBRACE] = "'{'",
    [TOK_RBRACE] = "'}'",
    [TOK_SEMI] = ";",
    [TOK_COMMA] = ",",
    [TOK_PLUS] = "+",
    [TOK_MINUS] = "-",
    [TOK_DOT] = ".",
    [TOK_ARROW] = "'->'",
    [TOK_AMP] = "&",
    [TOK_STAR] = "*",
    [TOK_SLASH] = "/",
    [TOK_PERCENT] = "%",
    [TOK_PIPE] = "|",
    [TOK_CARET] = "^",
    [TOK_SHL] = "'<<'",
    [TOK_SHR] = "'>>'",
    [TOK_PLUSEQ] = "+=",
    [TOK_MINUSEQ] = "-=",
    [TOK_STAREQ] = "*=",
    [TOK_SLASHEQ] = "/=",
    [TOK_PERCENTEQ] = "%=",
    [TOK_AMPEQ] = "&=",
    [TOK_PIPEEQ] = "|=",
    [TOK_CARETEQ] = "^=",
    [TOK_SHLEQ] = "<<=",
    [TOK_SHREQ] = ">>=",
    [TOK_INC] = "++",
    [TOK_DEC] = "--",
    [TOK_ASSIGN] = "=",
    [TOK_EQ] = "==",
    [TOK_NEQ] = "!=",
    [TOK_LOGAND] = "&&",
    [TOK_LOGOR] = "||",
    [TOK_NOT] = "!",
    [TOK_LT] = "<",
    [TOK_GT] = ">",
    [TOK_LE] = "<=",
    [TOK_GE] = ">=",
    [TOK_LBRACKET] = "[",
    [TOK_RBRACKET] = "]",
    [TOK_QMARK] = "?",
    [TOK_COLON] = ":",
    [TOK_LABEL] = "label",
    [TOK_ELLIPSIS] = "'...'",
    [TOK_UNKNOWN] = "unknown"
};

static const char *tok_name(token_type_t t)
{
    size_t n = sizeof(tok_names) / sizeof(tok_names[0]);
    if ((size_t)t < n && tok_names[t])
        return tok_names[t];
    return "unknown";
}

char *tokens_to_string(const token_t *toks, size_t count)
{
    if (!toks)
        return NULL;
    strbuf_t sb;
    strbuf_init(&sb);
    for (size_t i = 0; i < count; i++) {
        const token_t *tok = &toks[i];
        strbuf_appendf(&sb, "%zu:%zu %s %s\n", tok->line, tok->column,
                       tok_name(tok->type), tok->lexeme);
    }
    return sb.data;
}

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

    preproc_context_t ctx = {0};
    preproc_set_verbose_includes(cli->verbose_includes);
    ctx.max_include_depth = cli->max_include_depth;
    char *text = preproc_run(&ctx, path, incdirs, defines, undefines,
                             cli->sysroot, cli->vc_sysinclude,
                             cli->internal_libc);
    if (!text) {
        perror("preproc_run");
        unlink(path);
        free(path);
        return 0;
    }
    semantic_set_pack(ctx.pack_alignment);
    if (ctx.system_header)
        semantic_suppress_warnings = true;
    preproc_context_free(&ctx);

    *out_path = path;
    *out_text = text;
    return 1;
}

int compile_tokenize_impl(const char *source, const cli_options_t *cli,
                          const vector_t *incdirs, const vector_t *defines,
                          const vector_t *undefines, char **out_src,
                          token_t **out_toks, size_t *out_count,
                          char **tmp_path, vector_t *deps)
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
        preproc_context_t ctx = {0};
        preproc_set_verbose_includes(cli->verbose_includes);
        ctx.max_include_depth = cli->max_include_depth;
        text = preproc_run(&ctx, source, incdirs, defines, undefines,
                           cli->sysroot, cli->vc_sysinclude,
                           cli->internal_libc);
        if (!text) {
            perror("preproc_run");
            return 0;
        }
        if (deps) {
            for (size_t i = 0; i < ctx.deps.count; i++) {
                const char *p = ((const char **)ctx.deps.data)[i];
                char *dup = vc_strdup(p);
                if (!dup || !vector_push(deps, &dup)) {
                    free(dup);
                    preproc_context_free(&ctx);
                    free(text);
                    return 0;
                }
            }
        }
        semantic_set_pack(ctx.pack_alignment);
        if (ctx.system_header)
            semantic_suppress_warnings = true;
        preproc_context_free(&ctx);
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


#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include "preproc_builtin.h"
#include "util.h"


void preproc_set_location(preproc_context_t *ctx, const char *file,
                          size_t line, size_t column)
{
    if (file)
        ctx->file = file;
    ctx->line = line;
    ctx->column = column;
}

void preproc_set_function(preproc_context_t *ctx, const char *name)
{
    ctx->func = name;
}

void preproc_set_base_file(preproc_context_t *ctx, const char *file)
{
    ctx->base_file = file ? file : "";
}

void preproc_set_include_level(preproc_context_t *ctx, size_t level)
{
    ctx->include_level = level;
}

size_t preproc_get_line(const preproc_context_t *ctx)
{
    return ctx->line;
}

size_t preproc_get_column(const preproc_context_t *ctx)
{
    return ctx->column;
}

int handle_builtin_macro(const char *name, size_t len, size_t end,
                         size_t column, strbuf_t *out, size_t *pos,
                         preproc_context_t *ctx)
{
    if (len == 8) {
        if (strncmp(name, "__FILE__", 8) == 0) {
            preproc_set_location(ctx, NULL, ctx->line, column);
            strbuf_appendf(out, "\"%s\"", ctx->file);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__LINE__", 8) == 0) {
            preproc_set_location(ctx, NULL, ctx->line, column);
            strbuf_appendf(out, "%zu", ctx->line);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__DATE__", 8) == 0) {
            char buf[32];
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            strftime(buf, sizeof(buf), "%b %e %Y", &tm);
            preproc_set_location(ctx, NULL, ctx->line, column);
            strbuf_appendf(out, "\"%s\"", buf);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__TIME__", 8) == 0) {
            char buf[32];
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
            preproc_set_location(ctx, NULL, ctx->line, column);
            strbuf_appendf(out, "\"%s\"", buf);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__STDC__", 8) == 0) {
            preproc_set_location(ctx, NULL, ctx->line, column);
            strbuf_append(out, "1");
            *pos = end;
            return 1;
        } else if (strncmp(name, "__func__", 8) == 0) {
            if (ctx->func) {
                preproc_set_location(ctx, NULL, ctx->line, column);
                strbuf_appendf(out, "\"%s\"", ctx->func);
                *pos = end;
                return 1;
            }
        }
    } else if (len == 13 && strncmp(name, "__BASE_FILE__", 13) == 0) {
        preproc_set_location(ctx, NULL, ctx->line, column);
        strbuf_appendf(out, "\"%s\"", ctx->base_file);
        *pos = end;
        return 1;
    } else if (len == 11 && strncmp(name, "__COUNTER__", 11) == 0) {
        preproc_set_location(ctx, NULL, ctx->line, column);
        uint64_t value = ctx->counter++;
        if (ctx->counter == 0)
            fprintf(stderr, "Builtin counter overflow\n");
        strbuf_appendf(out, "%" PRIu64, value);
        *pos = end;
        return 1;
    } else if (len == 17 && strncmp(name, "__INCLUDE_LEVEL__", 17) == 0) {
        preproc_set_location(ctx, NULL, ctx->line, column);
        strbuf_appendf(out, "%zu", ctx->include_level);
        *pos = end;
        return 1;
    } else if (len == 16 && strncmp(name, "__STDC_VERSION__", 16) == 0) {
        preproc_set_location(ctx, NULL, ctx->line, column);
        strbuf_append(out, "199901L");
        *pos = end;
        return 1;
    }
    return 0;
}


#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <time.h>
#include "preproc_builtin.h"
#include "util.h"

static const char *builtin_file = "";
static size_t builtin_line = 0;
static size_t builtin_column = 1;
static const char *builtin_func = NULL;
static const char *builtin_base_file = "";
static size_t builtin_include_level = 0;
static unsigned long builtin_counter = 0;


void preproc_set_location(const char *file, size_t line, size_t column)
{
    if (file)
        builtin_file = file;
    builtin_line = line;
    builtin_column = column;
}

void preproc_set_function(const char *name)
{
    builtin_func = name;
}

void preproc_set_base_file(const char *file)
{
    builtin_base_file = file ? file : "";
}

void preproc_set_include_level(size_t level)
{
    builtin_include_level = level;
}

size_t preproc_get_line(void)
{
    return builtin_line;
}

size_t preproc_get_column(void)
{
    return builtin_column;
}

int handle_builtin_macro(const char *name, size_t len, size_t end,
                         size_t column, strbuf_t *out, size_t *pos)
{
    if (len == 8) {
        if (strncmp(name, "__FILE__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", builtin_file);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__LINE__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "%zu", builtin_line);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__DATE__", 8) == 0) {
            char buf[32];
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            strftime(buf, sizeof(buf), "%b %e %Y", &tm);
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", buf);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__TIME__", 8) == 0) {
            char buf[32];
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", buf);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__STDC__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_append(out, "1");
            *pos = end;
            return 1;
        } else if (strncmp(name, "__func__", 8) == 0) {
            if (builtin_func) {
                preproc_set_location(NULL, builtin_line, column);
                strbuf_appendf(out, "\"%s\"", builtin_func);
                *pos = end;
                return 1;
            }
        }
    } else if (len == 13 && strncmp(name, "__BASE_FILE__", 13) == 0) {
        preproc_set_location(NULL, builtin_line, column);
        strbuf_appendf(out, "\"%s\"", builtin_base_file);
        *pos = end;
        return 1;
    } else if (len == 11 && strncmp(name, "__COUNTER__", 11) == 0) {
        preproc_set_location(NULL, builtin_line, column);
        strbuf_appendf(out, "%lu", builtin_counter++);
        *pos = end;
        return 1;
    } else if (len == 17 && strncmp(name, "__INCLUDE_LEVEL__", 17) == 0) {
        preproc_set_location(NULL, builtin_line, column);
        strbuf_appendf(out, "%zu", builtin_include_level);
        *pos = end;
        return 1;
    } else if (len == 16 && strncmp(name, "__STDC_VERSION__", 16) == 0) {
        preproc_set_location(NULL, builtin_line, column);
        strbuf_append(out, "199901L");
        *pos = end;
        return 1;
    }
    return 0;
}


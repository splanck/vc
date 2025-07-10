#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "preproc_builtin.h"
#include "util.h"

static const char *builtin_file = "";
static size_t builtin_line = 0;
static size_t builtin_column = 1;
static const char *builtin_func = NULL;

static const char build_date[] = __DATE__;
static const char build_time[] = __TIME__;

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
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", build_date);
            *pos = end;
            return 1;
        } else if (strncmp(name, "__TIME__", 8) == 0) {
            preproc_set_location(NULL, builtin_line, column);
            strbuf_appendf(out, "\"%s\"", build_time);
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
    } else if (len == 16 && strncmp(name, "__STDC_VERSION__", 16) == 0) {
        preproc_set_location(NULL, builtin_line, column);
        strbuf_append(out, "199901L");
        *pos = end;
        return 1;
    }
    return 0;
}


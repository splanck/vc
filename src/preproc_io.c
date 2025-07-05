#include <stdlib.h>
#include <string.h>

#include "preproc_io.h"
#include "preproc_include.h"
#include "util.h"

/* Read a file and split it into NUL terminated lines.  The returned text
 * buffer backs the line array and must be freed along with the array. */
static char *read_file_lines(const char *path, char ***out_lines)
{
    char *text = vc_read_file(path);
    if (!text)
        return NULL;

    size_t len = strlen(text);
    size_t line_count = 1;
    for (char *p = text; *p; p++)
        if (*p == '\n')
            line_count++;
    if (len > 0 && text[len - 1] == '\n')
        line_count--;

    char **lines = vc_alloc_or_exit(sizeof(char *) * (line_count + 1));

    size_t idx = 0;
    lines[idx++] = text;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            text[i] = '\0';
            if (i + 1 < len)
                lines[idx++] = &text[i + 1];
        }
    }
    lines[idx] = NULL;
    *out_lines = lines;
    return text;
}

/* Load "path" into memory and register it on the include stack. */
int load_source(const char *path, vector_t *stack,
                char ***out_lines, char **out_dir, char **out_text)
{
    char **lines;
    char *text = read_file_lines(path, &lines);
    if (!text)
        return 0;

    char *dir = NULL;
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        dir = vc_strndup(path, len);
        if (!dir) {
            free(lines);
            free(text);
            return 0;
        }
    }

    if (!include_stack_push(stack, path)) {
        free(lines);
        free(text);
        free(dir);
        return 0;
    }

    *out_lines = lines;
    *out_dir = dir;
    *out_text = text;
    return 1;
}

/* Free resources obtained from load_source. */
void cleanup_source(char *text, char **lines, char *dir)
{
    free(lines);
    free(text);
    free(dir);
}


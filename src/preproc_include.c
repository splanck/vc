#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "preproc_include.h"
#include "util.h"

/* Standard system include directories */
static const char *std_include_dirs[] = {
    "/usr/local/include",
    "/usr/include",
    NULL
};

/* Files marked with #pragma once */
static vector_t pragma_once_files;

void preproc_include_init(void)
{
    vector_init(&pragma_once_files, sizeof(char *));
}

void preproc_include_cleanup(void)
{
    for (size_t i = 0; i < pragma_once_files.count; i++)
        free(((char **)pragma_once_files.data)[i]);
    vector_free(&pragma_once_files);
}

int include_stack_contains(vector_t *stack, const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon) {
        if (errno != ENOENT)
            perror(path);
        return 0;
    }
    for (size_t i = 0; i < stack->count; i++) {
        const char *p = ((const char **)stack->data)[i];
        if (strcmp(p, canon) == 0) {
            free(canon);
            return 1;
        }
    }
    free(canon);
    return 0;
}

int include_stack_push(vector_t *stack, const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon) {
        canon = vc_strdup(path);
        if (!canon) {
            fprintf(stderr, "Out of memory\n");
            return 0;
        }
    }
    if (!vector_push(stack, &canon)) {
        free(canon);
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    return 1;
}

void include_stack_pop(vector_t *stack)
{
    if (stack->count) {
        free(((char **)stack->data)[stack->count - 1]);
        stack->count--;
    }
}

int pragma_once_contains(const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon)
        canon = vc_strdup(path);
    if (!canon)
        return 0;
    for (size_t i = 0; i < pragma_once_files.count; i++) {
        const char *p = ((const char **)pragma_once_files.data)[i];
        if (strcmp(p, canon) == 0) {
            free(canon);
            return 1;
        }
    }
    free(canon);
    return 0;
}

int pragma_once_add(const char *path)
{
    char *canon = realpath(path, NULL);
    if (!canon)
        canon = vc_strdup(path);
    if (!canon)
        return 0;
    for (size_t i = 0; i < pragma_once_files.count; i++) {
        const char *p = ((const char **)pragma_once_files.data)[i];
        if (strcmp(p, canon) == 0) {
            free(canon);
            return 1;
        }
    }
    if (!vector_push(&pragma_once_files, &canon)) {
        free(canon);
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    return 1;
}

char *find_include_path(const char *fname, char endc,
                        const char *dir, const vector_t *incdirs)
{
    size_t fname_len = strlen(fname);
    size_t max_len = fname_len;
    if (endc == '"' && dir) {
        size_t len = strlen(dir) + fname_len;
        if (len > max_len)
            max_len = len;
    }

    for (size_t i = 0; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        size_t len = strlen(base) + 1 + fname_len;
        if (len > max_len)
            max_len = len;
    }

    for (size_t i = 0; std_include_dirs[i]; i++) {
        size_t len = strlen(std_include_dirs[i]) + 1 + fname_len;
        if (len > max_len)
            max_len = len;
    }

    char *out_path = vc_alloc_or_exit(max_len + 1);

    if (endc == '"' && dir) {
        snprintf(out_path, max_len + 1, "%s%s", dir, fname);
        if (access(out_path, R_OK) == 0)
            return out_path;
    }

    for (size_t i = 0; i < incdirs->count; i++) {
        const char *base = ((const char **)incdirs->data)[i];
        snprintf(out_path, max_len + 1, "%s/%s", base, fname);
        if (access(out_path, R_OK) == 0)
            return out_path;
    }

    if (endc == '<') {
        for (size_t i = 0; std_include_dirs[i]; i++) {
            snprintf(out_path, max_len + 1, "%s/%s", std_include_dirs[i], fname);
            if (access(out_path, R_OK) == 0)
                return out_path;
        }
        free(out_path);
        return NULL;
    }

    snprintf(out_path, max_len + 1, "%s", fname);
    if (access(out_path, R_OK) == 0)
        return out_path;

    for (size_t i = 0; std_include_dirs[i]; i++) {
        snprintf(out_path, max_len + 1, "%s/%s", std_include_dirs[i], fname);
        if (access(out_path, R_OK) == 0)
            return out_path;
    }

    free(out_path);
    return NULL;
}


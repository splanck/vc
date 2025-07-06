/*
 * Miscellaneous utility functions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include "util.h"
#include "preproc_macros.h"

#ifndef __has_attribute
# define __has_attribute(x) 0
#endif

/*
 * Provide a weak stub for macro_free so unit tests linking only a subset
 * of the source files do not fail due to an unresolved symbol.  When the
 * real implementation from preproc_macros.c is linked in, it overrides this
 * weak definition.
 */
#if __has_attribute(weak)
__attribute__((weak)) void macro_free(macro_t *m) { (void)m; }
__attribute__((weak)) void vector_free(vector_t *v) { (void)v; }
#else
void __attribute__((weak)) macro_free(macro_t *m) { (void)m; }
void __attribute__((weak)) vector_free(vector_t *v) { (void)v; }
#endif

/* Print a generic out of memory message */
void vc_oom(void)
{
    fprintf(stderr, "Out of memory\n");
}

/*
 * Allocate "size" bytes of memory.  If the allocation fails the process
 * prints an error message and terminates.  The returned block is
 * uninitialised.
 */
void *vc_alloc_or_exit(size_t size)
{
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "vc: out of memory\n");
        exit(1);
    }
    return p;
}

/*
 * Reallocate a block previously obtained from malloc.  Behaviour mirrors
 * realloc() except that failure results in program termination.
 */
void *vc_realloc_or_exit(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (!p) {
        fprintf(stderr, "vc: out of memory\n");
        exit(1);
    }
    return p;
}

/* Return a newly allocated copy of the given NUL terminated string. */
char *vc_strdup(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/* Duplicate at most "n" characters of a string. */
char *vc_strndup(const char *s, size_t n)
{
    size_t len = strlen(s);
    if (len > n)
        len = n;
    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/*
 * Read the entire contents of a file into a newly allocated buffer.  A
 * trailing NUL byte is always appended.  The caller must free the
 * returned pointer with free().  NULL is returned on I/O errors.
 */
char *vc_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }
    if (fseeko(f, 0, SEEK_END) != 0) {
        perror("fseeko");
        fclose(f);
        return NULL;
    }
    off_t len_off = ftello(f);
    if (len_off < 0) {
        perror("ftello");
        fclose(f);
        return NULL;
    }
    if ((uintmax_t)len_off > SIZE_MAX) {
        fprintf(stderr, "vc: file too large\n");
        fclose(f);
        return NULL;
    }
    if (fseeko(f, 0, SEEK_SET) != 0) {
        perror("fseeko");
        fclose(f);
        return NULL;
    }
    size_t len = (size_t)len_off;
    char *buf = vc_alloc_or_exit(len + 1);
    if (fread(buf, 1, len, f) != len) {
        perror("fread");
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/*
 * Convert string to size_t using strtoul with overflow checking.
 */
int vc_strtoul_size(const char *s, size_t *out)
{
    errno = 0;
    char *end;
    unsigned long val = strtoul(s, &end, 10);
    if (errno == ERANGE || val > SIZE_MAX || *end != '\0')
        return 0;
    *out = (size_t)val;
    return 1;
}

/*
 * Convert string to unsigned with overflow checking.
 */
int vc_strtoul_unsigned(const char *s, unsigned *out)
{
    errno = 0;
    char *end;
    unsigned long val = strtoul(s, &end, 10);
    if (errno == ERANGE || val > UINT_MAX || *end != '\0')
        return 0;
    *out = (unsigned)val;
    return 1;
}

/*
 * Release all strings stored in a vector and free the vector itself.
 *
 * Each element of the vector must be a pointer returned by malloc.
 */
void free_string_vector(vector_t *v)
{
    if (!v)
        return;
    for (size_t i = 0; i < v->count; i++)
        free(((char **)v->data)[i]);
    vector_free(v);
}

/*
 * Release all macros stored in a vector and free the vector itself.
 *
 * Each macro must have been created by add_macro() so that macro_free()
 * can correctly release the heap allocated strings.
 */
void free_macro_vector(vector_t *v)
{
    if (!v)
        return;
    for (size_t i = 0; i < v->count; i++)
        macro_free(&((macro_t *)v->data)[i]);
    vector_free(v);
}


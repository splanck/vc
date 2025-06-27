/*
 * Miscellaneous utility functions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util.h"

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
    char *out = vc_alloc_or_exit(len + 1);
    memcpy(out, s, len + 1);
    return out;
}

/* Duplicate at most "n" characters of a string. */
char *vc_strndup(const char *s, size_t n)
{
    char *out = vc_alloc_or_exit(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
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
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (len < 0) {
        perror("ftell");
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }
    char *buf = vc_alloc_or_exit((size_t)len + 1);
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        perror("fread");
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}


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
#ifndef PATH_MAX
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
#include <unistd.h>
#include <fcntl.h>
#include "util.h"
#include "preproc_macros.h"

#ifdef UNIT_TESTING
/*
 * Provide stub implementations so unit test binaries that only link a
 * subset of the sources still resolve these symbols.
 */
void macro_free(macro_t *m) { (void)m; }
#ifndef NO_VECTOR_FREE_STUB
void vector_free(vector_t *v) { (void)v; }
#endif
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
    while (*end == 'u' || *end == 'U' || *end == 'l' || *end == 'L')
        end++;
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
    while (*end == 'u' || *end == 'U' || *end == 'l' || *end == 'L')
        end++;
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

/*
 * Assemble mkstemp template path using cli->obj_dir (or the process
 * temporary directory) and the given prefix.  Returns a newly allocated
 * string or NULL on error.
 *
 * errno will be ENAMETOOLONG if the resulting path would exceed PATH_MAX
 * or snprintf detected truncation.
 */
char *
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
 * Build a template using create_temp_template, open the file with mkstemp and
 * set FD_CLOEXEC.  On success the file descriptor is returned and *out_path is
 * set to the allocated path.  The caller must unlink and free *out_path when
 * done.  On failure -1 is returned, *out_path is set to NULL and errno
 * indicates the error.
 */
int
create_temp_file(const cli_options_t *cli, const char *prefix, char **out_path)
{
    char *tmpl = create_temp_template(cli, prefix);
    if (!tmpl) {
        *out_path = NULL;
        return -1;
    }

    int fd = mkstemp(tmpl);
    if (fd < 0) {
        free(tmpl);
        *out_path = NULL;
        return -1;
    }
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        int err = errno;
        close(fd);
        unlink(tmpl);
        free(tmpl);
        errno = err;
        *out_path = NULL;
        return -1;
    }

    *out_path = tmpl;
    return fd;
}


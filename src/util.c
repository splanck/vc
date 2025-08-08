/*
 * Miscellaneous utility functions.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#define _GNU_SOURCE
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
#include "ast_stmt.h"
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
void ast_free_func(func_t *func) { (void)func; }
void ast_free_stmt(stmt_t *stmt) { (void)stmt; }
#endif

/* Print a generic out of memory message */
void vc_oom(void)
{
    fprintf(stderr, "Out of memory\n");
}

/*
 * Allocate 'size' bytes of memory using malloc().
 *
 * The argument specifies the number of bytes to reserve.  If the allocation
 * fails an error message is printed to stderr and the program terminates.
 * As a result the returned pointer is guaranteed to be non-NULL and refers to
 * an uninitialised block of the requested size.
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
 * Resize a heap allocation previously obtained from malloc().
 *
 * 'ptr' is the existing allocation and may be NULL to request a new block,
 * 'size' is the desired size in bytes.  Behaviour otherwise mirrors
 * realloc() except that if the reallocation fails an error message is
 * printed and the process terminates.  Consequently this function never
 * returns NULL.
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
    if (!s)
        return NULL;
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
    if (!s)
        return NULL;
    size_t len = strnlen(s, n);
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
    if (s[0] == '-')
        return 0;
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
    if (s[0] == '-')
        return 0;
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
 * Release all functions stored in a vector and free the vector itself.
 *
 * Each element must be a func_t pointer created by the AST constructors.
 */
void free_func_list_vector(vector_t *v)
{
    if (!v)
        return;
    for (size_t i = 0; i < v->count; i++)
        ast_free_func(((func_t **)v->data)[i]);
    vector_free(v);
}

/*
 * Release all global statements stored in a vector and free the vector itself.
 *
 * Each element must be a stmt_t pointer created by the AST constructors.
 */
void free_glob_list_vector(vector_t *v)
{
    if (!v)
        return;
    for (size_t i = 0; i < v->count; i++)
        ast_free_stmt(((stmt_t **)v->data)[i]);
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

    int n = snprintf(tmpl, len + 1, "%s/%sXXXXXX", dir, prefix);
    if (n < 0) {
        int err = errno;
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
 * Create and open the temporary file described by tmpl.  The file is opened
 * with O_CLOEXEC using mkostemp when available; otherwise mkstemp is used and
 * FD_CLOEXEC is set via fcntl.  Returns the file descriptor on success or -1
 * on failure.  On error the file is unlinked and errno is preserved.
 */
int
open_temp_file(char *tmpl)
{
#if defined(_GNU_SOURCE) ||                                    \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700)
    int fd = mkostemp(tmpl, O_CLOEXEC);
    if (fd < 0)
        return -1;
    return fd;
#else
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        int err = errno;
        close(fd);
        unlink(tmpl);
        errno = err;
        return -1;
    }
    return fd;
#endif
}


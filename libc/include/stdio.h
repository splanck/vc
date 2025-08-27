/*
 * Minimal stdio declarations for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STDIO_H
#define VC_STDIO_H

#include <stdarg.h>

/* Returns the number of characters written including the newline. If the
 * length would overflow an int, INT_MAX is returned instead. */
int puts(const char *s);
/* Returns the number of characters written. If the length would overflow an
 * int, INT_MAX is returned instead. */
int printf(const char *format, ...);

struct FILE_struct {
    int fd;
    int err;
    int eof;
};
#define FILE struct FILE_struct

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
/* Returns the number of characters written. If the length would overflow an
 * int, INT_MAX is returned instead. */
int fprintf(FILE *stream, const char *format, ...);
char *fgets(char *s, int size, FILE *stream);
FILE *tmpfile(void);
void perror(const char *msg);


#endif /* VC_STDIO_H */

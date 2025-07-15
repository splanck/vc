/*
 * Minimal stdio declarations for vc's internal libc.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#ifndef VC_STDIO_H
#define VC_STDIO_H

#include <stdarg.h>

int puts(const char *s);
int printf(const char *format, ...);

struct FILE_struct {
    int fd;
};
#define FILE struct FILE_struct

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
char *fgets(char *s, int size, FILE *stream);
void perror(const char *msg);


#endif /* VC_STDIO_H */

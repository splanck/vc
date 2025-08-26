#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cli_env.h"
#include "util.h"

int
count_vcflags_args(const char *env, size_t *out)
{
    char *tmp = vc_strdup(env);
    if (!tmp) {
        fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
        return 1;
    }

    size_t count = 0;
    char *p = tmp;
    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        count++;
        int in_quote = 0;
        char q = '\0';
        size_t bs = 0;
        while (*p) {
            if (*p == '\\') {
                bs++;
                p++;
                continue;
            }
            if (*p == '\'' || *p == '"') {
                if ((bs & 1) == 0) {
                    /* quote not escaped */
                    bs = 0;
                    if (!in_quote) {
                        in_quote = 1;
                        q = *p++;
                        continue;
                    }
                    if (*p == q) {
                        in_quote = 0;
                        p++;
                        continue;
                    }
                } else {
                    /* escaped quote */
                    bs = 0;
                    p++;
                    continue;
                }
            }
            if (!in_quote && *p == ' ')
                break;
            bs = 0;
            p++;
        }
        if (in_quote) {
            fprintf(stderr, "Unterminated quote in VCFLAGS\n");
            free(tmp);
            return 1;
        }
        while (*p && *p != ' ')
            p++;
    }
    free(tmp);
    *out = count;
    return 0;
}

char **
build_vcflags_argv(char *vcbuf, int argc, char **argv, size_t vcargc)
{
    size_t new_count = (size_t)argc + vcargc + 1;
    if (new_count > SIZE_MAX / sizeof(char *)) {
        fprintf(stderr, "vc: argument vector too large\n");
        return NULL;
    }

    char **vcargv = malloc(sizeof(char *) * new_count);
    if (!vcargv) {
        fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
        return NULL;
    }

    vcargv[0] = argv[0];
    size_t idx = 1;
    char *p2 = vcbuf;
    while (*p2) {
        while (*p2 == ' ')
            p2++;
        if (!*p2)
            break;
        char *start = p2;
        char *dst = p2;
        int in_quote = 0;
        char q = '\0';
        char delim = '\0';
        size_t bs = 0;
        while (*p2) {
            if (*p2 == '\\') {
                bs++;
                p2++;
                continue;
            }
            if (*p2 == '\'' || *p2 == '"') {
                for (size_t i = 0; i < bs / 2; i++)
                    *dst++ = '\\';
                if (bs & 1) {
                    *dst++ = *p2++;
                } else {
                    if (!in_quote) {
                        in_quote = 1;
                        q = *p2++;
                        bs = 0;
                        continue;
                    } else if (*p2 == q) {
                        in_quote = 0;
                        p2++;
                        bs = 0;
                        continue;
                    } else {
                        *dst++ = *p2++;
                    }
                }
                bs = 0;
                continue;
            }
            while (bs) {
                *dst++ = '\\';
                bs--;
            }
            if (!in_quote && *p2 == ' ') {
                delim = *p2;
                break;
            }
            *dst++ = *p2++;
        }
        while (bs) {
            *dst++ = '\\';
            bs--;
        }
        if (in_quote) {
            fprintf(stderr, "Unterminated quote in VCFLAGS\n");
            free(vcargv);
            return NULL;
        }
        *dst = '\0';
        vcargv[idx++] = start;
        if (delim)
            p2++; /* skip delimiter */
        while (*p2 == ' ')
            p2++;
    }

    for (int i = 1; i < argc; i++)
        vcargv[idx++] = argv[i];

    vcargv[idx] = NULL;
    return vcargv;
}

int load_vcflags(int *argc, char ***argv, char ***out_argv,
                 char **out_buf)
{
    char **vcargv = NULL;
    char *vcbuf = NULL;
    const char *env = getenv("VCFLAGS");
    if (env && *env) {
        size_t vcargc;
        if (count_vcflags_args(env, &vcargc))
            return 1;

        vcbuf = vc_strdup(env);
        if (!vcbuf) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            return 1;
        }

        vcargv = build_vcflags_argv(vcbuf, *argc, *argv, vcargc);
        if (!vcargv) {
            free(vcbuf);
            return 1;
        }

        *argv = vcargv;
        *argc += (int)vcargc;
    }

    *out_argv = vcargv;
    *out_buf = vcbuf;
    return 0;
}

static int match_flag(const char *arg, const char *flag)
{
    if (strcmp(arg, flag) == 0)
        return 1;

    size_t len = strlen(arg);
    if (len >= 2 && (arg[0] == '"' || arg[0] == '\'') && arg[len - 1] == arg[0]) {
        char *buf = vc_strndup(arg + 1, len - 2);
        if (!buf) {
            vc_oom();
            return 0;
        }
        int match = strcmp(buf, flag) == 0;
        free(buf);
        if (match)
            return 1;
    }

    return 0;
}

void scan_shortcuts(int *argc, char **argv)
{
    int new_argc = 1;
    for (int i = 1; i < *argc; i++) {
        if (match_flag(argv[i], "-MD"))
            argv[new_argc++] = "--MD";
        else if (match_flag(argv[i], "-M"))
            argv[new_argc++] = "--M";
        else
            argv[new_argc++] = argv[i];
    }
    *argc = new_argc;
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cli_env.h"
#include "util.h"

int load_vcflags(int *argc, char ***argv, char ***out_argv,
                 char **out_buf)
{
    char **vcargv = NULL;
    char *vcbuf = NULL;
    size_t vcargc = 0;
    const char *env = getenv("VCFLAGS");
    if (env && *env) {
        vcbuf = vc_strdup(env);
        if (!vcbuf) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            return 1;
        }

        char *tmp = vc_strdup(env);
        if (!tmp) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            free(vcbuf);
            return 1;
        }
        char *p = tmp;
        while (*p) {
            while (*p == ' ')
                p++;
            if (!*p)
                break;
            vcargc++;
            int in_quote = 0;
            char q = '\0';
            while (*p) {
                if (*p == '\\' && p[1]) {
                    p += 2;
                    continue;
                }
                if (!in_quote && (*p == '\'' || *p == '"')) {
                    in_quote = 1;
                    q = *p++;
                    continue;
                }
                if (in_quote && *p == q) {
                    in_quote = 0;
                    p++;
                    continue;
                }
                if (!in_quote && *p == ' ')
                    break;
                p++;
            }
            while (*p && *p != ' ')
                p++;
        }
        free(tmp);

        size_t new_count = (size_t)*argc + vcargc + 1;
        if (new_count > SIZE_MAX / sizeof(char *)) {
            fprintf(stderr, "vc: argument vector too large\n");
            free(vcbuf);
            return 1;
        }

        size_t alloc_sz = sizeof(char *) * new_count;
        vcargv = malloc(alloc_sz);
        if (!vcargv) {
            fprintf(stderr, "Out of memory while processing VCFLAGS.\n");
            free(vcbuf);
            return 1;
        }

        vcargv[0] = (*argv)[0];
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
            while (*p2) {
                if (*p2 == '\\' && p2[1]) {
                    p2++;
                    *dst++ = *p2++;
                    continue;
                }
                if (!in_quote && (*p2 == '\'' || *p2 == '"')) {
                    in_quote = 1;
                    q = *p2++;
                    continue;
                }
                if (in_quote && *p2 == q) {
                    in_quote = 0;
                    p2++;
                    continue;
                }
                if (!in_quote && *p2 == ' ') {
                    delim = *p2;
                    break;
                }
                *dst++ = *p2++;
            }
            *dst = '\0';
            vcargv[idx++] = start;
            if (delim)
                p2++; /* skip delimiter */
            while (*p2 == ' ')
                p2++;
        }
        for (int i = 1; i < *argc; i++)
            vcargv[idx++] = (*argv)[i];

        vcargv[idx] = NULL;

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
        if (buf) {
            int match = strcmp(buf, flag) == 0;
            free(buf);
            if (match)
                return 1;
        }
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

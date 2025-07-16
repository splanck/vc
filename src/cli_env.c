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
            if (*p == '\'' || *p == '"') {
                char q = *p++;
                while (*p && *p != q)
                    p++;
                if (*p)
                    p++;
            } else {
                while (*p && *p != ' ')
                    p++;
            }
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
            char *start;
            if (*p2 == '\'' || *p2 == '"') {
                char q = *p2++;
                start = p2;
                while (*p2 && *p2 != q)
                    p2++;
                if (*p2)
                    *p2++ = '\0';
            } else {
                start = p2;
                while (*p2 && *p2 != ' ')
                    p2++;
                if (*p2)
                    *p2++ = '\0';
            }
            vcargv[idx++] = start;
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
        char buf[8];
        if (len - 2 < sizeof(buf)) {
            memcpy(buf, arg + 1, len - 2);
            buf[len - 2] = '\0';
            if (strcmp(buf, flag) == 0)
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

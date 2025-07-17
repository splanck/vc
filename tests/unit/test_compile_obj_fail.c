#include <string.h>
#include <errno.h>
#include "cli.h"

int compile_source_obj(const char *source, const cli_options_t *cli,
                       char **out_path);

/* provide stub so compile_source_obj can link but is never called */
int test_compile_unit(const char *src, const cli_options_t *cli,
                      const char *out, int compile_obj)
{
    (void)src; (void)cli; (void)out; (void)compile_obj;
    return 1;
}

/* force mkstemp/mkostemp failure */
int mkstemp(char *template)
{
    (void)template;
    errno = EACCES;
    return -1;
}

int mkostemp(char *template, int flags)
{
    (void)template; (void)flags;
    errno = EACCES;
    return -1;
}

int main(void)
{
    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    char *path = (char *)0x1;
    int ok = compile_source_obj("dummy.c", &cli, &path);
    if (ok || path != (char *)0x1)
        return 1;
    return 0;
}

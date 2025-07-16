#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifndef PATH_MAX
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
#include "cli.h"
#include "vector.h"

/* build_and_link_objects is exposed when UNIT_TESTING is defined */
int build_and_link_objects(vector_t *objs, const cli_options_t *cli);

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

/* stub startup object creator */
int test_create_startup_object(const cli_options_t *cli, int use_x86_64, char **out_path)
{
    (void)cli; (void)use_x86_64;
    *out_path = strdup("stub.o");
    return *out_path != NULL;
}

/* stub command runner */
int test_command_run(char **argv)
{
    (void)argv;
    return 1;
}

int main(void)
{
    cli_options_t cli;
    memset(&cli, 0, sizeof(cli));
    cli.internal_libc = 1;
    cli.vc_sysinclude = malloc(PATH_MAX + 100);
    memset(cli.vc_sysinclude, 'a', PATH_MAX + 99);
    cli.vc_sysinclude[PATH_MAX + 99] = '\0';
    cli.output = "out";

    vector_t objs;
    vector_init(&objs, sizeof(char *));
    char *dummy = strdup("dummy.o");
    vector_push(&objs, &dummy);

    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    int ok = build_and_link_objects(&objs, &cli);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    free(dummy);
    for (size_t i = 0; i < objs.count; i++)
        free(((char **)objs.data)[i]);
    vector_free(&objs);
    free(cli.vc_sysinclude);

    ASSERT(!ok);
    ASSERT(strstr(buf, "internal libc path too long") != NULL);

    if (failures == 0)
        printf("All vc_sysinclude_long tests passed\n");
    else
        printf("%d vc_sysinclude_long test(s) failed\n", failures);
    return failures ? 1 : 0;
}

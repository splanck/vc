#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "preproc_path.h"
#include "util.h"

/* stub popen that always fails */
FILE *test_popen(const char *cmd, const char *mode)
{
    (void)cmd; (void)mode;
    errno = ENOSYS;
    return NULL;
}

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    unsetenv("VCPATH");
    unsetenv("VCINC");
    unsetenv("CPATH");
    unsetenv("C_INCLUDE_PATH");

    char tmpdir[] = "/tmp/vc_intlibXXXXXX";
    char *idir = mkdtemp(tmpdir);
    ASSERT(idir != NULL);
    setenv("VC_SYSINCLUDE", idir, 1);
    preproc_set_internal_libc_dir(idir);

    vector_t empty; vector_init(&empty, sizeof(char *));
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        exit(1);
    }
    int saved = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    vector_t dirs;
    ASSERT(collect_include_dirs(&dirs, &empty, "/tmp/sysroot", NULL, true));

    FILE *list = tmpfile();
    ASSERT(list != NULL);
    print_include_search_dirs(list, '<', NULL, &dirs, 0);
    fflush(list);
    fseek(list, 0, SEEK_SET);
    char lbuf[512];
    size_t ln = fread(lbuf, 1, sizeof(lbuf) - 1, list);
    lbuf[ln] = '\0';
    fclose(list);
    int count = 0;
    const char *p = lbuf;
    while ((p = strstr(p, idir))) {
        count++;
        p += strlen(idir);
    }
    ASSERT(count == 1);

    fflush(stderr);
    fseek(tmp, 0, SEEK_SET);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';

    dup2(saved, fileno(stderr));
    close(saved);
    fclose(tmp);

    free_string_vector(&dirs);
    vector_free(&empty);
    preproc_path_cleanup();
    unsetenv("VC_SYSINCLUDE");
    rmdir(idir);

    ASSERT(strstr(buf, "system headers could not be located") != NULL);

    if (failures == 0)
        printf("All preproc_sysheaders_fail tests passed\n");
    else
        printf("%d preproc_sysheaders_fail test(s) failed\n", failures);
    return failures ? 1 : 0;
}

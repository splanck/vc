#ifndef VC_CLI_ENV_H
#define VC_CLI_ENV_H

int load_vcflags(int *argc, char ***argv, char ***out_argv, char **out_buf);
int count_vcflags_args(const char *env, size_t *out);
char **build_vcflags_argv(char *vcbuf, int argc, char **argv,
                          size_t vcargc);
void scan_shortcuts(int *argc, char **argv);

#endif /* VC_CLI_ENV_H */

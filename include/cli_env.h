#ifndef VC_CLI_ENV_H
#define VC_CLI_ENV_H

int load_vcflags(int *argc, char ***argv, char ***out_argv, char **out_buf);
void scan_shortcuts(int *argc, char **argv);

#endif /* VC_CLI_ENV_H */

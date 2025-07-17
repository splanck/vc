#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "cli_opts.h"
#include "preproc_path.h"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define VERSION "0.1.0"
#include "util.h"

/* usage message shared by multiple handlers */
void print_usage(const char *prog)
{
    static const char *usage_lines[] = {
        "Usage: %s [options] <source...>\n",
        "Options:\n",
        "  -o, --output <file>  Output path\n",
        "  -O<N>               Optimization level (0-3)\n",
        "  -I, --include <dir> Add directory to include search path\n",
        "  -L<dir>             Add library search path\n",
        "  -l<name>            Link against library\n",
        "  -Dname[=val]       Define a macro\n",
        "  -Uname             Undefine a macro\n",
        "  -fmax-include-depth=<n>  Set maximum include depth\n",
        "      --std=<std>      Language standard (c99 or gnu99)\n",
        "  -h, --help           Display this help and exit\n",
        "  -v, --version        Print version information and exit\n",
        "  -c, --compile        Assemble to an object file\n",
        "      --link           Compile and link to an executable\n",
        "      --obj-dir <dir>  Directory for temporary object files\n",
        "      --sysroot <dir>  Prefix system include paths with <dir>\n",
        "      --vc-sysinclude <dir>  Prepend <dir> to system headers\n",
        "      --internal-libc   Use bundled libc headers\n",
        "      --verbose-includes  Print include search details\n",
        "      --no-fold        Disable constant folding\n",
        "      --no-dce         Disable dead code elimination\n",
        "      --no-cprop       Disable constant propagation\n",
        "      --no-inline      Disable inline expansion\n",
        "      --debug          Emit .file/.loc directives\n",
        "      --no-color       Disable colored diagnostics\n",
        "      --no-warn-unreachable  Disable unreachable code warnings\n",
        "      --x86-64         Generate 64-bit x86 assembly\n",
        "      --intel-syntax    Use Intel assembly syntax\n",
        "      --emit-dwarf      Include DWARF information\n",
        "  -S, --dump-asm       Print assembly to stdout and exit\n",
        "      --dump-ir        Print IR to stdout and exit\n",
        "      --dump-tokens    Print tokens to stdout and exit\n",
        "  -M                   Generate dependency file and exit\n",
        "  -MD                  Generate dependency file during compilation\n",
        "  -E, --preprocess     Run only the preprocessor and print the result\n",
        "  Provide '-' as a source file to read from standard input.\n",
    };

    for (size_t i = 0; i < sizeof(usage_lines) / sizeof(usage_lines[0]); i++) {
        if (i == 0)
            printf(usage_lines[i], prog);
        else
            printf("%s", usage_lines[i]);
    }
}

static int push_source(cli_options_t *opts, const char *src)
{
    if (!vector_push(&opts->sources, &src)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int add_include_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->include_dirs, &dir)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int add_lib_dir(cli_options_t *opts, const char *dir)
{
    if (!vector_push(&opts->lib_dirs, &dir)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int add_library(cli_options_t *opts, const char *name)
{
    if (!vector_push(&opts->libs, &name)) {
        vc_oom();
        return -1;
    }
    return 0;
}

static int set_opt_level(cli_options_t *opts, const char *level)
{
    errno = 0;
    char *end;
    long long val = strtoll(level, &end, 10);
    if (*end != '\0' || errno != 0 || val < 0 || val > INT_MAX || val > 3) {
        fprintf(stderr, "Invalid optimization level '%s'\n", level);
        return 1;
    }

    opts->opt_cfg.opt_level = (int)val;
    if (opts->opt_cfg.opt_level <= 0) {
        opts->opt_cfg.fold_constants = 0;
        opts->opt_cfg.dead_code = 0;
        opts->opt_cfg.const_prop = 0;
        opts->opt_cfg.inline_funcs = 0;
    } else {
        opts->opt_cfg.fold_constants = 1;
        opts->opt_cfg.dead_code = 1;
        opts->opt_cfg.const_prop = 1;
        opts->opt_cfg.inline_funcs = 1;
    }

    return 0;
}

static int set_standard(cli_options_t *opts, const char *std)
{
    if (strcmp(std, "c99") == 0)
        opts->std = STD_C99;
    else if (strcmp(std, "gnu99") == 0)
        opts->std = STD_GNU99;
    else {
        fprintf(stderr, "Unknown standard '%s'\n", std);
        return 1;
    }
    return 0;
}

static int set_max_depth(cli_options_t *opts, const char *val)
{
    errno = 0;
    char *end;
    long long v = strtoll(val, &end, 10);
    if (*end != '\0' || errno != 0 || v <= 0 || v > INT_MAX) {
        fprintf(stderr, "Invalid include depth '%s'\n", val);
        return 1;
    }
    opts->max_include_depth = (size_t)v;
    return 0;
}

static int handle_help(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)opts;
    print_usage(prog);
    exit(0);
}

static int handle_version(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)arg; (void)prog; (void)opts;
    printf("vc version %s\n", VERSION);
    exit(0);
}

static int add_define_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -D option\n");
        return 1;
    }
    if (!vector_push(&opts->defines, &arg)) {
        vc_oom();
        return 1;
    }
    return 0;
}

static int add_undef_opt(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    if (!arg || !*arg) {
        fprintf(stderr, "Missing argument for -U option\n");
        return 1;
    }
    if (!vector_push(&opts->undefines, &arg)) {
        vc_oom();
        return 1;
    }
    return 0;
}

static int handle_std(const char *arg, const char *prog, cli_options_t *opts)
{
    (void)prog;
    return set_standard(opts, arg);
}

/* Handle debug related flags
 * --debug sets opts->debug
 * --emit-dwarf sets opts->emit_dwarf
 */
static int handle_debug_flags(int opt, cli_options_t *opts)
{
    switch (opt) {
    case CLI_OPT_DEBUG:
        opts->debug = true;
        return 0;
    case CLI_OPT_EMIT_DWARF:
        opts->emit_dwarf = true;
        return 0;
    default:
        return -1;
    }
}

/* Handle output dump flags
 * -S/--dump-asm   sets opts->dump_asm
 * --dump-ast      sets opts->dump_ast
 * --dump-ir       sets opts->dump_ir
 * --dump-tokens   sets opts->dump_tokens
 */
static int handle_dump_flags(int opt, cli_options_t *opts)
{
    switch (opt) {
    case 'S':
    case CLI_OPT_DUMP_ASM_LONG:
        opts->dump_asm = true;
        return 0;
    case CLI_OPT_DUMP_AST:
        opts->dump_ast = true;
        return 0;
    case CLI_OPT_DUMP_IR:
        opts->dump_ir = true;
        return 0;
    case CLI_OPT_DUMP_TOKENS:
        opts->dump_tokens = true;
        return 0;
    default:
        return -1;
    }
}

/* Simple boolean flags whose values are set via a table */
struct flag_entry {
    int opt;
    void (*set)(cli_options_t *opts);
};

static void set_compile(cli_options_t *opts) { opts->compile = true; }
static void set_link(cli_options_t *opts) { opts->link = true; }
static void set_x86(cli_options_t *opts) { opts->use_x86_64 = true; }
static void set_intel(cli_options_t *opts) { opts->asm_syntax = ASM_INTEL; }
static void set_preprocess(cli_options_t *opts) { opts->preprocess = true; }
static void set_no_color(cli_options_t *opts) { opts->color_diag = false; }
static void set_dep_only(cli_options_t *opts) { opts->dep_only = true; }
static void set_dep(cli_options_t *opts) { opts->deps = true; }
static void set_no_warn(cli_options_t *opts) { opts->warn_unreachable = false; }
static void set_verbose(cli_options_t *opts) { opts->verbose_includes = true; }
static void set_named_locals(cli_options_t *opts) { opts->named_locals = true; }


int parse_optimization_opts(int opt, const char *arg, cli_options_t *opts)
{
    switch (opt) {
    case 'O':
        return set_opt_level(opts, arg);
    case CLI_OPT_NO_FOLD:
        opts->opt_cfg.fold_constants = 0;
        return 0;
    case CLI_OPT_NO_DCE:
        opts->opt_cfg.dead_code = 0;
        return 0;
    case CLI_OPT_NO_CPROP:
        opts->opt_cfg.const_prop = 0;
        return 0;
    case CLI_OPT_NO_INLINE:
        opts->opt_cfg.inline_funcs = 0;
        return 0;
    default:
        return -1;
    }
}

int parse_io_paths(int opt, const char *arg, cli_options_t *opts)
{
    switch (opt) {
    case 'o':
        opts->output = (char *)arg;
        return 0;
    case 'I':
        return add_include_dir(opts, arg);
    case 'L':
        return add_lib_dir(opts, arg);
    case 'l':
        return add_library(opts, arg);
    case CLI_OPT_OBJ_DIR:
        opts->obj_dir = (char *)arg;
        return 0;
    case CLI_OPT_SYSROOT:
        opts->sysroot = (char *)arg;
        return 0;
    case CLI_OPT_VC_SYSINCLUDE:
        opts->vc_sysinclude = (char *)arg;
        return 0;
    case CLI_OPT_INTERNAL_LIBC:
        opts->internal_libc = true;
        return 0;
    default:
        return -1;
    }
}

int parse_misc_opts(int opt, const char *arg, const char *prog,
                    cli_options_t *opts)
{
    static const struct flag_entry table[] = {
        { 'c', set_compile },
        { CLI_OPT_LINK, set_link },
        { CLI_OPT_X86_64, set_x86 },
        { CLI_OPT_INTEL_SYNTAX, set_intel },
        { 'E', set_preprocess },
        { CLI_OPT_NO_COLOR, set_no_color },
        { CLI_OPT_DEP_ONLY, set_dep_only },
        { CLI_OPT_DEP, set_dep },
        { CLI_OPT_NO_WARN_UNREACHABLE, set_no_warn },
        { CLI_OPT_VERBOSE_INCLUDES, set_verbose },
        { CLI_OPT_NAMED_LOCALS, set_named_locals },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].opt == opt) {
            table[i].set(opts);
            return 0;
        }
    }

    if (handle_dump_flags(opt, opts) == 0)
        return 0;
    if (handle_debug_flags(opt, opts) == 0)
        return 0;

    switch (opt) {
    case 'h':
        return handle_help(arg, prog, opts);
    case 'v':
        return handle_version(arg, prog, opts);
    case 'D':
    case CLI_OPT_DEFINE:
        return add_define_opt(arg, prog, opts);
    case 'U':
    case CLI_OPT_UNDEFINE:
        return add_undef_opt(arg, prog, opts);
    case 'f':
        if (strncmp(arg, "max-include-depth=", 18) == 0)
            return set_max_depth(opts, arg + 18);
        fprintf(stderr, "Unknown -f option '%s'\n", arg);
        return 1;
    case CLI_OPT_STD:
        return handle_std(arg, prog, opts);
    case CLI_OPT_FMAX_DEPTH:
        return set_max_depth(opts, arg);
    default:
        return -1;
    }
}

/* Collect remaining command line arguments as source files */
static int collect_sources(int argc, char **argv, const char *prog,
                           cli_options_t *opts)
{
    if (optind >= argc) {
        fprintf(stderr, "Error: no source file specified.\n");
        print_usage(prog);
        cli_free_opts(opts);
        return 1;
    }

    for (int i = optind; i < argc; i++) {
        if (push_source(opts, argv[i])) {
            cli_free_opts(opts);
            return 1;
        }
    }

    return 0;
}

/* Check that an output option or dump flag was provided */
static int validate_output(const char *prog, cli_options_t *opts)
{
    if (!opts->output && !opts->dump_asm && !opts->dump_ir &&
        !opts->dump_tokens && !opts->dump_ast && !opts->preprocess &&
        !opts->dep_only) {
        fprintf(stderr, "Error: no output path specified.\n");
        print_usage(prog);
        cli_free_opts(opts);
        return 1;
    }

    return 0;
}

/* Configure the bundled libc when --internal-libc is used */
static int setup_internal_libc(const char *prog, cli_options_t *opts)
{
    if (!opts->internal_libc)
        return 0;

    if (!opts->vc_sysinclude || !*opts->vc_sysinclude) {
        char tmp[PATH_MAX];
        size_t prog_len = strlen(prog);
        if (prog_len >= sizeof(tmp)) {
            fprintf(stderr, "Error: internal libc path too long.\n");
            cli_free_opts(opts);
            return 1;
        }
        memcpy(tmp, prog, prog_len + 1);
        char *slash = strrchr(tmp, '/');
        if (slash)
            *slash = '\0';
        else
            strcpy(tmp, ".");
        size_t dirlen = strlen(tmp);
        if (dirlen + strlen("/libc/include") >= PATH_MAX) {
            fprintf(stderr, "Error: internal libc path too long.\n");
            cli_free_opts(opts);
            return 1;
        }
        strcat(tmp, "/libc/include");
        opts->vc_sysinclude = vc_strdup(tmp);
        if (!opts->vc_sysinclude) {
            vc_oom();
            cli_free_opts(opts);
            return 1;
        }
        opts->free_vc_sysinclude = true;
    }
    preproc_set_internal_libc_dir(opts->vc_sysinclude);

    const char *dir = opts->vc_sysinclude;
    int ret;
    char hdr[PATH_MAX];
    ret = snprintf(hdr, sizeof(hdr), "%s/stdio.h", dir);
    if (ret < 0 || ret >= (int)sizeof(hdr) || access(hdr, F_OK) != 0) {
        fprintf(stderr, "Error: internal libc header '%s' not found.\n", hdr);
        cli_free_opts(opts);
        return 1;
    }

    char libdir[PATH_MAX];
    ret = snprintf(libdir, sizeof(libdir), "%s", dir);
    if (ret < 0 || ret >= (int)sizeof(libdir)) {
        fprintf(stderr, "Error: internal libc archive path too long.\n");
        cli_free_opts(opts);
        return 1;
    }
    char *slash = strrchr(libdir, '/');
    if (slash)
        *slash = '\0';
    const char *libname = opts->use_x86_64 ? "libc64.a" : "libc32.a";
    char archive[PATH_MAX];
    if (snprintf(archive, sizeof(archive), "%s/%s", libdir, libname) >=
        (int)sizeof(archive)) {
        fprintf(stderr, "Error: internal libc archive path too long.\n");
        cli_free_opts(opts);
        return 1;
    }
    if (access(archive, F_OK) != 0) {
        fprintf(stderr, "Error: internal libc archive '%s' not found.\n",
                archive);
        cli_free_opts(opts);
        return 1;
    }

    return 0;
}

int finalize_options(int argc, char **argv, const char *prog,
                     cli_options_t *opts)
{
    if (collect_sources(argc, argv, prog, opts))
        return 1;

    if (validate_output(prog, opts))
        return 1;

    if (setup_internal_libc(prog, opts))
        return 1;

    return 0;
}


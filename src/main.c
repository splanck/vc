#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "token.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"
#include "opt.h"
#include "codegen.h"

#define VERSION "0.1.0"

static void print_usage(const char *prog)
{
    printf("Usage: %s [options] <source>\n", prog);
    printf("Options:\n");
    printf("  -o, --output <file>  Output path\n");
    printf("  -h, --help           Display this help and exit\n");
    printf("  -v, --version        Print version information and exit\n");
    printf("      --no-fold        Disable constant folding\n");
    printf("      --no-dce         Disable dead code elimination\n");
    printf("      --x86-64         Generate 64-bit x86 assembly\n");
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    static struct option long_opts[] = {
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {"output",  required_argument, 0, 'o'},
        {"no-fold", no_argument,       0, 1},
        {"no-dce",  no_argument,       0, 2},
        {"x86-64", no_argument,       0, 3},
        {0, 0, 0, 0}
    };

    char *output = NULL;
    int opt;
    opt_config_t opt_cfg = {1, 1};
    int use_x86_64 = 0;

    while ((opt = getopt_long(argc, argv, "hvo:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 'v':
            printf("vc version %s\n", VERSION);
            return 0;
        case 'o':
            output = optarg;
            break;
        case 1:
            opt_cfg.fold_constants = 0;
            break;
        case 2:
            opt_cfg.dead_code = 0;
            break;
        case 3:
            use_x86_64 = 1;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: no source file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (!output) {
        fprintf(stderr, "Error: no output path specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    const char *source = argv[optind];

    char *src_text = read_file(source);
    if (!src_text) {
        perror("read_file");
        return 1;
    }

    size_t tok_count = 0;
    token_t *tokens = lexer_tokenize(src_text, &tok_count);
    if (!tokens) {
        fprintf(stderr, "Tokenization failed\n");
        free(src_text);
        return 1;
    }

    parser_t parser;
    parser_init(&parser, tokens, tok_count);
    symtable_t funcs;
    symtable_t globals;
    symtable_init(&funcs);
    symtable_init(&globals);
    ir_builder_t ir;
    ir_builder_init(&ir);

    int ok = 1;
    size_t fcap = 4, fcount = 0;
    size_t gcap = 4, gcount = 0;
    func_t **func_list = malloc(fcap * sizeof(*func_list));
    stmt_t **glob_list = malloc(gcap * sizeof(*glob_list));
    if (!func_list || !glob_list)
        ok = 0;
    while (ok && !parser_is_eof(&parser)) {
        func_t *fn = NULL;
        stmt_t *g = NULL;
        if (!parser_parse_toplevel(&parser, &fn, &g)) {
            fprintf(stderr, "Parse error\n");
            ok = 0;
            break;
        }
        if (fn) {
            if (fcount >= fcap) {
                fcap *= 2;
                func_t **tmp = realloc(func_list, fcap * sizeof(*tmp));
                if (!tmp) {
                    ok = 0;
                    ast_free_func(fn);
                    break;
                }
                func_list = tmp;
            }
            func_list[fcount++] = fn;
        } else if (g) {
            if (gcount >= gcap) {
                gcap *= 2;
                stmt_t **tmp = realloc(glob_list, gcap * sizeof(*tmp));
                if (!tmp) {
                    ok = 0;
                    ast_free_stmt(g);
                    break;
                }
                glob_list = tmp;
            }
            glob_list[gcount++] = g;
        }
    }

    for (size_t i = 0; i < fcount; i++)
        symtable_add(&funcs, func_list[i]->name, func_list[i]->return_type);
    for (size_t i = 0; i < gcount; i++)
        ok = ok && check_global(glob_list[i], &globals, &ir);

    for (size_t i = 0; i < fcount && ok; i++) {
        if (!check_func(func_list[i], &funcs, &globals, &ir)) {
            fprintf(stderr, "Semantic error\n");
            ok = 0;
        }
    }

    /* Run optimizations on the generated IR */
    if (ok)
        opt_run(&ir, &opt_cfg);

    /* Generate assembly output */
    if (ok) {
        FILE *outf = fopen(output, "w");
        if (!outf) {
            perror("fopen");
            ok = 0;
        } else {
            codegen_emit_x86(outf, &ir, use_x86_64);
            fclose(outf);
        }
    }

    ir_builder_free(&ir);

    for (size_t i = 0; i < fcount; i++)
        ast_free_func(func_list[i]);
    for (size_t i = 0; i < gcount; i++)
        ast_free_stmt(glob_list[i]);
    free(func_list);
    free(glob_list);

    symtable_free(&funcs);
    symtable_free(&globals);
    lexer_free_tokens(tokens, tok_count);
    free(src_text);

    if (ok)
        printf("Compiling %s -> %s\n", source, output);

    return ok ? 0 : 1;
}

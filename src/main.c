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
        {0, 0, 0, 0}
    };

    char *output = NULL;
    int opt;

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
    symtable_t syms;
    symtable_init(&syms);
    ir_builder_t ir;
    ir_builder_init(&ir);

    int ok = 1;
    while (!parser_is_eof(&parser)) {
        stmt_t *stmt = parser_parse_stmt(&parser);
        if (!stmt) {
            fprintf(stderr, "Parse error\n");
            ok = 0;
            break;
        }
        if (!check_stmt(stmt, &syms, &ir)) {
            fprintf(stderr, "Semantic error\n");
            ok = 0;
            ast_free_stmt(stmt);
            break;
        }
        ast_free_stmt(stmt);
    }

    /* Run optimizations on the generated IR */
    if (ok)
        opt_run(&ir);

    /* Generate assembly output */
    if (ok) {
        FILE *outf = fopen(output, "w");
        if (!outf) {
            perror("fopen");
            ok = 0;
        } else {
            codegen_emit_x86(outf, &ir);
            fclose(outf);
        }
    }

    ir_builder_free(&ir);

    symtable_free(&syms);
    lexer_free_tokens(tokens, tok_count);
    free(src_text);

    if (ok)
        printf("Compiling %s -> %s\n", source, output);

    return ok ? 0 : 1;
}

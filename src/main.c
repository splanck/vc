#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define VERSION "0.1.0"

static void print_usage(const char *prog)
{
    printf("Usage: %s [options] <source>\n", prog);
    printf("Options:\n");
    printf("  -o, --output <file>  Output path\n");
    printf("  -h, --help           Display this help and exit\n");
    printf("  -v, --version        Print version information and exit\n");
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

    /* Placeholder for compilation logic */
    printf("Compiling %s -> %s\n", source, output);

    return 0;
}

#include <stdio.h>
#include "error.h"

static size_t error_line = 0;
static size_t error_column = 0;

void error_set(size_t line, size_t col)
{
    error_line = line;
    error_column = col;
}

void error_print(const char *msg)
{
    fprintf(stderr, "%s at line %zu, column %zu\n", msg, error_line, error_column);
}


#include <stdio.h>
#include "ast_expr.h"
#include "consteval.h"
#include "symtable.h"
#include "util.h"

static int failures = 0;
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    symtable_t tab; symtable_init(&tab);
    struct_member_t members[2] = {
        {"a", TYPE_INT, 4, 0, 0, 0, 0},
        {"b", TYPE_CHAR, 1, 4, 0, 0, 0}
    };
    symtable_add_struct(&tab, "S", members, 2);

    char **names = malloc(sizeof(char *));
    names[0] = vc_strdup("b");
    expr_t *e = ast_make_offsetof(TYPE_STRUCT, "S", names, 1, 1, 1);
    long long val = 0;
    ASSERT(eval_const_expr(e, &tab, 0, &val));
    ASSERT(val == 4);
    ast_free_expr(e);
    symtable_free(&tab);
    if (failures == 0)
        printf("All eval_offsetof tests passed\n");
    else
        printf("%d eval_offsetof test(s) failed\n", failures);
    return failures ? 1 : 0;
}

#include "label.h"

static int next_label_id = 0;

void label_init(void)
{
    next_label_id = 0;
}

int label_next_id(void)
{
    return next_label_id++;
}

void label_reset(void)
{
    next_label_id = 0;
}

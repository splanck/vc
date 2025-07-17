#include "errno.h"

/*
 * Provide storage for errno and a helper returning its address.
 * In this simple libc there is only one global instance.
 */
static int errno_value;

int *_errno_location(void)
{
    return &errno_value;
}

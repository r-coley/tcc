/* tests/control/test_shortcircuit_null.c */

#include <string.h>

int main(void)
{
    char *p = 0;

    if (p && strcmp(p, "hello") == 0)
        return 1;

    return 42;
}

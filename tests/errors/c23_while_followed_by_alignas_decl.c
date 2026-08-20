#include <stdalign.h>

int
main(void)
{
    while (0)
        alignas(16) int x = 1;
    return 0;
}

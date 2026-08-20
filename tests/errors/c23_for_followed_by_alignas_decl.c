#include <stdalign.h>

int
main(void)
{
    for (;;)
        alignas(16) int x = 1;
    return 0;
}

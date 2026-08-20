#include <stdalign.h>

int
main(void)
{
    if (0)
        return 0;
    else
        alignas(16) int x = 1;
    return 0;
}

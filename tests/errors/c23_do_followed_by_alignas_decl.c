#include <stdalign.h>

int
main(void)
{
    do
        alignas(16) int x = 1;
    while (0);
    return 0;
}

#include <stddef.h>

int
main(void)
{
    do
        nullptr_t p = nullptr;
    while (0);
    return p == nullptr;
}

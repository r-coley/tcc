#include <stddef.h>

int
main(void)
{
    for (;;)
        nullptr_t p = nullptr;
    return p == nullptr;
}

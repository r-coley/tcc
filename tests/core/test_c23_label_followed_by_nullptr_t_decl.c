#include <stddef.h>

int
main(void)
{
label:
    nullptr_t p = nullptr;
    return p == 0 ? 42 : 1;
}

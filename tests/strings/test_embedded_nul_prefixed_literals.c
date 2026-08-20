#include <wchar.h>

int
main(void)
{
    wchar_t w[] = L"a\0b";
    short *u = u"a\0b";
    int *U = U"a\0b";

    if (sizeof(L"a\0b") != 16)
        return 1;
    if (w[0] != 'a' || w[1] != 0 || w[2] != 'b' || w[3] != 0)
        return 2;

    if (sizeof(u"a\0b") != 8)
        return 3;
    if (u[0] != 'a' || u[1] != 0 || u[2] != 'b' || u[3] != 0)
        return 4;

    if (sizeof(U"a\0b") != 16)
        return 5;
    if (U[0] != 'a' || U[1] != 0 || U[2] != 'b' || U[3] != 0)
        return 6;

    return 42;
}

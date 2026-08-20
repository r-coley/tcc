#include <wchar.h>

int
main(void)
{
    wchar_t *w = L"A" L"Z";
    short *u = u"h" u"i";
    int *U = U"O" U"K";

    if (sizeof(L"A" L"Z") != 12)
        return 1;
    if (w[0] != 'A' || w[1] != 'Z' || w[2] != 0)
        return 2;

    if (sizeof(u"h" u"i") != 6)
        return 3;
    if (u[0] != 'h' || u[1] != 'i' || u[2] != 0)
        return 4;

    if (sizeof(U"O" U"K") != 12)
        return 5;
    if (U[0] != 'O' || U[1] != 'K' || U[2] != 0)
        return 6;

    return 42;
}

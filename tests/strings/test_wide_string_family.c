#include <wchar.h>

int main(void) {
    wchar_t *w = L"AZ";
    short *u = u"hi";
    int *U = U"Q";

    if (sizeof(L"AZ") != 12)
        return 1;
    if (w[0] != 'A' || w[1] != 'Z' || w[2] != 0)
        return 2;

    if (sizeof(u"hi") != 6)
        return 3;
    if (u[0] != 'h' || u[1] != 'i' || u[2] != 0)
        return 4;

    if (sizeof(U"Q") != 8)
        return 5;
    if (U[0] != 'Q' || U[1] != 0)
        return 6;

    if (L'X' != 'X' || u'Y' != 'Y' || U'Z' != 'Z')
        return 7;

    return 42;
}

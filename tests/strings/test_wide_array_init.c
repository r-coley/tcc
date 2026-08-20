/* Regression: wchar_t a[] = L"..." must store wchar_t elements, not bytes */
#include <wchar.h>

extern int strcmp(const char *, const char *);

int main(void) {
    /* Local wide array from string literal */
    wchar_t a[] = L"Hello";
    if (a[0] != 'H') return 1;
    if (a[1] != 'e') return 2;
    if (a[2] != 'l') return 3;
    if (a[3] != 'l') return 4;
    if (a[4] != 'o') return 5;
    if (a[5] != 0)   return 6;

    /* Local wide array from unicode escape string */
    wchar_t b[] = L"\u0048\u0065\u006C\u006C\u006F";
    if (b[0] != 'H') return 7;
    if (b[1] != 'e') return 8;
    if (b[4] != 'o') return 9;

    /* Wide array with non-ASCII codepoints */
    wchar_t c[] = L"\u20AC\u00A3";  /* Euro, Pound */
    if (c[0] != 8364) return 10;
    if (c[1] != 163)  return 11;
    if (c[2] != 0)    return 12;

    return 42;
}

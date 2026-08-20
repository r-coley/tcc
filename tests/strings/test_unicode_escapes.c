/* Regression: \uXXXX and \UXXXXXXXX unicode escapes in string/char literals */
#include <wchar.h>

extern int strcmp(const char *, const char *);

int main(void) {
    /* Wide char literals */
    wchar_t a = L'\u0041';      /* U+0041 = 'A' = 65 */
    wchar_t b = L'\U00000042';  /* U+0042 = 'B' = 66 */
    wchar_t euro = L'\u20AC';   /* Euro sign U+20AC = 8364 */

    if (a != 65)   return 1;
    if (b != 66)   return 2;
    if (euro != 8364) return 3;

    /* Narrow string with \u (ASCII range) */
    char cs[] = "\u0041\u0042\u0043";  /* "ABC" */
    if (cs[0] != 'A') return 4;
    if (cs[1] != 'B') return 5;
    if (cs[2] != 'C') return 6;

    /* Wide string pointer with \u */
    wchar_t *ws = L"\u0048\u0069";  /* "Hi" */
    if (ws[0] != 72)  return 7;   /* 'H' */
    if (ws[1] != 105) return 8;   /* 'i' */

    /* Wide string pointer with \U */
    wchar_t *ws2 = L"\U00000048\U00000069"; /* "Hi" */
    if (ws2[0] != 72)  return 9;
    if (ws2[1] != 105) return 10;

    /* 'x' = 0x78: would crash if treated as pointer */
    wchar_t xchar = L'\u0078';
    if (xchar != 120) return 11;

    return 42;
}

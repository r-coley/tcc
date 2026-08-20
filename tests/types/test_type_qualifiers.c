/* Regression for parsing C type qualifiers. */

int main(void) {
    volatile int x = 1;
    const int y = 2;
    int volatile z = 3;
    int plain = 3;
    const char *p = "ok";
    char * volatile q = (char *)p;
    int * restrict rp;
    _Atomic int ax = 4;

    rp = &plain;

    if (x != 1)
        return 1;
    if (y != 2)
        return 2;
    if (z != 3)
        return 3;
    if (q[0] != 'o' || q[1] != 'k')
        return 4;
    if (*rp != 3)
        return 5;
    if (ax != 4)
        return 6;

    return 42;
}

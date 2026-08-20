int main(void) {
    unsigned long long a;
    unsigned long long b;
    unsigned long long c;

    a = 0xff00ff00ULL;
    b = 0x00ff00ffULL;
    c = (a | b) ^ 0xffffffffULL;

    if (c != 0ULL)
        return 1;

    c = (a & 0xff000000ULL) >> 24;

    if (c != 0xffULL)
        return 2;

    return 0;
}

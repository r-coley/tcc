int main(void) {
    unsigned long long x;
    unsigned long long y;

    x = 1ULL;
    y = x << 33;

    if (y != 0x200000000ULL)
        return 1;

    y = y >> 32;

    if (y != 2ULL)
        return 2;

    return 0;
}

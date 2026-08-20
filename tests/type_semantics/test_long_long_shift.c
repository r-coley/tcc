/* 64-bit shifts, including unsigned logical right shift. */
int main(void)
{
    unsigned long long one = 1ULL;
    unsigned long long high = 0x8000000000000000ULL;
    int n = 32;

    if ((one << n) != 0x100000000ULL) return 1;
    if ((one << 63) != high) return 2;
    if ((high >> 63) != 1ULL) return 3;
    if ((0xF000000000000000ULL >> 60) != 0xFULL) return 4;

    long long neg = -8LL;
    if ((neg >> 1) != -4LL) return 5;

    return 42;
}

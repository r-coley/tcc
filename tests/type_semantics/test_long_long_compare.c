/* 64-bit signed/unsigned comparison edge cases. */
int main(void)
{
    long long neg = -1LL;
    long long zero = 0LL;
    long long big = 0x100000000LL;

    if (!(neg < zero)) return 1;
    if (!(big > zero)) return 2;
    if (neg > big) return 3;

    unsigned long long high = 0x8000000000000000ULL;
    unsigned long long low = 1ULL;

    if (!(high > low)) return 4;
    if (high < low) return 5;
    if (!(0xFFFFFFFFFFFFFFFFULL >= high)) return 6;
    if (!(0ULL < high)) return 7;

    return 42;
}

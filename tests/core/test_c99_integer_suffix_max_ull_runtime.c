int
main(void)
{
    unsigned long long a = 18446744073709551615ULL;
    unsigned long long b = 18446744073709551615LLU;
    unsigned long long c = 18446744073709551615uLL;
    unsigned long long d = 18446744073709551615ull;

    if (sizeof(a) != 8)
        return 1;
    if (a != b || b != c || c != d)
        return 2;
    if ((a >> 63) != 1ULL)
        return 3;
    if ((a + 1ULL) != 0ULL)
        return 4;

    return 42;
}

/* 64-bit signed and unsigned division/modulo. */
int main(void)
{
    long long a = 0x300000000LL;
    long long b = 3LL;
    if (a / b != 0x100000000LL) return 1;
    if (a % b != 0LL) return 2;
    if ((-17LL / 5LL) != -3LL) return 3;
    if ((-17LL % 5LL) != -2LL) return 4;

    unsigned long long high = 0xFFFFFFFFFFFFFFF0ULL;
    if (high / 16ULL != 0x0FFFFFFFFFFFFFFFULL) return 5;
    if (high % 16ULL != 0ULL) return 6;

    return 42;
}

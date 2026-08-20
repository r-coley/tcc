/* long long arithmetic: values that overflow 32 bits */
int main(void)
{
    long long a = 0x100000000LL;  /* 4GB */
    long long b = 0x100000001LL;
    if (a + 1 != b) return 1;
    if (b - a != 1) return 2;
    if (a * 2 != 0x200000000LL) return 3;

    unsigned long long u = 0xFFFFFFFFFFFFFFFFULL;
    if (u + 1 != 0ULL) return 4;  /* wraps to 0 */

    long long neg = -1LL;
    if (neg >= 0) return 5;

    return 42;
}

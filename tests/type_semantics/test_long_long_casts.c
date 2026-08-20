/* 64-bit casts and truncation. */
int main(void)
{
    int minus_one = -1;
    long long sx = (long long)minus_one;
    unsigned long long ux = (unsigned long long)(unsigned int)minus_one;

    if (sx != -1LL) return 1;
    if (ux != 0xFFFFFFFFULL) return 2;

    unsigned long long wide = 0x1122334455667788ULL;
    if ((int)wide != 0x55667788) return 3;

    int obj = 123;
    unsigned long as_ul = (unsigned long)&obj;
    unsigned long long as_ull = (unsigned long long)&obj;
    if ((int *)as_ul != &obj) return 4;
    if ((int *)as_ull != &obj) return 5;

    return 42;
}

/* long long function argument and return value */
long long add_ll(long long a, long long b)
{
    return a + b;
}

long long identity_ll(long long x)
{
    return x;
}

int main(void)
{
    long long r = add_ll(0x100000000LL, 0x200000000LL);
    if (r != 0x300000000LL) return 1;

    long long v = identity_ll(0xDEADBEEFCAFELL);
    if (v != 0xDEADBEEFCAFELL) return 2;

    /* negative */
    if (add_ll(-1LL, 1LL) != 0LL) return 3;

    return 42;
}

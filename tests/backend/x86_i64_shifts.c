long long shl64(long long x, int n) {
    return x << n;
}

long long sar64(long long x, int n) {
    return x >> n;
}

unsigned long long shr64(unsigned long long x, int n) {
    return x >> n;
}

int main(void) {
    long long a;
    long long b;
    unsigned long long c;
    unsigned long long d;

    a = shl64(3LL, 5);
    b = sar64(-64LL, 2);
    c = shr64(0x8000000000000000ULL, 63);
    d = shr64(0xffff000000000000ULL, 48);

    if (a != 96LL)
        return 1;
    if (b != -16LL)
        return 2;
    if (c != 1ULL)
        return 3;
    if (d != 65535ULL)
        return 4;

    return 0;
}

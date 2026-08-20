struct Inner64 {
    unsigned long long u;
};

struct Holder64 {
    char c;
    long long s;
    struct Inner64 inner;
};

int main(void)
{
    struct Holder64 h;
    h.c = 3;
    h.s = -0x100000000LL;
    h.inner.u = 0x8000000000000001ULL;

    if (h.c != 3) return 1;
    if (h.s != -0x100000000LL) return 2;
    if (h.inner.u != 0x8000000000000001ULL) return 3;
    if ((h.inner.u >> 63) != 1ULL) return 4;
    return 42;
}

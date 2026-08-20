long long sign_extend_int(int x) {
    return (long long)x;
}

unsigned long long zero_extend_uint(unsigned int x) {
    return (unsigned long long)x;
}

int truncate_i64(long long x) {
    return (int)x;
}

unsigned int truncate_ui64(unsigned long long x) {
    return (unsigned int)x;
}

int ptr_roundtrip(void) {
    int value;
    int *p;
    unsigned long long raw;
    int *q;

    value = 1234;
    p = &value;
    raw = (unsigned long long)p;
    q = (int *)raw;

    return *q;
}

int main(void) {
    long long a;
    unsigned long long b;

    a = sign_extend_int(-7);
    b = zero_extend_uint(0xffffffffU);

    if (a != -7)
        return 1;
    if (b != 4294967295ULL)
        return 2;
    if (truncate_i64(0x10000002aLL) != 42)
        return 3;
    if (truncate_ui64(0x1000000ffULL) != 255U)
        return 4;
    if (ptr_roundtrip() != 1234)
        return 5;
    return 0;
}

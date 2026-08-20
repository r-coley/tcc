long long add64(long long a, long long b) {
    return a + b;
}

long long sub64(long long a, long long b) {
    return a - b;
}

int main(void) {
    long long x;
    long long y;

    x = 0x100000000LL;
    y = 42LL;

    if (add64(x, y) != 0x10000002aLL)
        return 1;

    if (sub64(add64(x, y), y) != x)
        return 2;

    return 0;
}

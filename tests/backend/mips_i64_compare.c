int main(void) {
    long long a;
    long long b;
    unsigned long long ua;
    unsigned long long ub;

    a = -5LL;
    b = 42LL;
    ua = 0xffffffffULL;
    ub = 0x100000000ULL;

    if (!(a < b))
        return 1;
    if (!(b > a))
        return 2;
    if (!(ua < ub))
        return 3;
    if (!(ub > ua))
        return 4;

    return 0;
}

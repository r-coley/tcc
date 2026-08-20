long long and64(long long a, long long b) {
    return a & b;
}

long long or64(long long a, long long b) {
    return a | b;
}

long long xor64(long long a, long long b) {
    return a ^ b;
}

long long not64(long long a) {
    return ~a;
}

int main(void) {
    long long a;
    long long b;
    long long c;
    long long d;

    a = and64(0x00ff00ff00ff00ffLL, 0x0f0f0f0f0f0f0f0fLL);
    b = or64(0x00000000ffff0000LL, 0x0000ffff00000000LL);
    c = xor64(0x00ff00ff00ff00ffLL, 0x0000ffff0000ffffLL);
    d = not64(0LL);

    if (a != 0x000f000f000f000fLL)
        return 1;
    if (b != 0x0000ffffffff0000LL)
        return 2;
    if (c != 0x00ffff0000ffff00LL)
        return 3;
    if (d != -1LL)
        return 4;

    return 0;
}

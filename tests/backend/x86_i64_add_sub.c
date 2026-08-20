long long f(long long a, long long b) {
    long long c;
    c = a + b;
    c = c - 3;
    return c;
}

int main(void) {
    long long x;
    x = f(10LL, 20LL);
    return (int)x;
}

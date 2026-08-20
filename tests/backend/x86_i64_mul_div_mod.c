long long f(long long a, long long b) {
    long long x;
    long long y;
    long long z;

    x = a * b;
    y = x / 3;
    z = x % 5;

    return y + z;
}

int main(void) {
    long long r;
    r = f(6LL, 7LL);
    return (int)r;
}

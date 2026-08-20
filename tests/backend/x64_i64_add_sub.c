long long f(long long a, long long b) {
    long long x;
    x = a + b;
    x = x - 5;
    return x;
}

int main(void) {
    long long r;
    r = f(40LL, 7LL);
    return (int)r;
}

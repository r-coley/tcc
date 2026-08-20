long long f(long long a, long long b) {
    if (a < b)
        return 11;
    if (a == b)
        return 22;
    return 33;
}

int main(void) {
    return (int)f(40LL, 42LL);
}

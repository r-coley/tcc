int f(long long a, long long b) {
    if (a < b)
        return 11;
    if (a == b)
        return 22;
    return 33;
}

int main(void) {
    return f(10LL, 20LL);
}

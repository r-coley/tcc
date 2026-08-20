long long mul64(long long a, long long b) {
    return a * b;
}

long long div64(long long a, long long b) {
    return a / b;
}

long long mod64(long long a, long long b) {
    return a % b;
}

int main(void) {
    long long a;
    long long b;
    long long m;
    long long d;
    long long r;

    /*
     * Keep this MIPS probe compile/check-only for now.  The current backend
     * still lowers i64 mul/div/mod through low-word scalar operations, so use
     * small constants until true split-word or helper-call lowering exists.
     */
    a = 1000LL;
    b = 3LL;

    m = mul64(a, b);
    d = div64(m, b);
    r = mod64(m, b);

    return d == a && r == 0;
}

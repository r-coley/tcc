/* Regression for -O1 strength reduction of multiplication by powers of two. */

static int scale4(int x) {
    return x * 4;
}

static int scale8(int x) {
    return x * 8;
}

int main(void) {
    int a = scale4(7);
    int b = scale8(3);

    if (a != 28)
        return 1;
    if (b != 24)
        return 2;

    return 42;
}

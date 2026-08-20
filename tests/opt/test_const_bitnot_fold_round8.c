/* Regression for constant folding through bitwise-not in ROUND8-style expressions. */

static int round8_87(void) {
    return (0x57 + 7) & ~7;
}

static int round8_sizeof_long(void) {
    return (sizeof(long) + 7) & ~7;
}

int main(void) {
    if (round8_87() != 88)
        return 1;
    if (round8_sizeof_long() != 8)
        return 2;
    return 42;
}

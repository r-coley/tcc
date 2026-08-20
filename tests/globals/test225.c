/* Regression: fixed-size static short array initializers use the right element size. */
static short s[3] = { 10, 20, 12 };

int main(void) {
    return s[0] + s[1] + s[2];
}

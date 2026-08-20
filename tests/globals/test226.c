/* Regression: fixed-size static long array initializers use the right element size. */
static long l[3] = { 10, 20, 12 };

int main(void) {
    return l[0] + l[1] + l[2];
}

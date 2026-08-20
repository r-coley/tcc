/* Regression: static char array brace initializers preserve embedded zeroes and zero-fill. */
static char c[5] = { 1, 0, 2 };

int main(void) {
    return c[0] * 10 + c[1] + c[2] + c[3] + c[4] + 30;
}

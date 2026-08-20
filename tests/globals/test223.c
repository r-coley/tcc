/* Regression: fixed-size static int array initializers must zero-fill trailing elements. */
static int a[5] = { 1, 2, 3 };

int main(void) {
    return a[0] + a[1] + a[2] + a[3] + a[4] + 36;
}

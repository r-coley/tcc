/* Regression for running CFG dead-branch elimination at -O1. */

static int f(int x) {
    if (0) {
        x = x + 1000;
        return x;
    }

    return x + 1;
}

int main(void) {
    if (f(41) != 42)
        return 1;

    return 42;
}

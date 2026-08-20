/* Regression tests for the C comma operator in expression contexts. */

static int ret_comma(void) {
    int x = 1;
    return x += 2, x += 3, x;
}

int main(void) {
    int i;
    int j;
    int sum;
    int x;

    x = (1, 2);
    if (x != 2)
        return 1;

    x = 0;
    x = x + 1, x = x + 2, x = x + 3;
    if (x != 6)
        return 2;

    sum = 0;
    for (i = 0, j = 0; i < 5; i++, j += 2)
        sum += j;

    if (i != 5)
        return 3;
    if (j != 10)
        return 4;
    if (sum != 20)
        return 5;

    if ((x = 0, x = 9, x) != 9)
        return 6;

    if (ret_comma() != 6)
        return 7;

    x = 0;
    if (x = 1, x)
        x = 3;
    if (x != 3)
        return 8;

    while (x = x - 1, x > 0)
        ;
    if (x != 0)
        return 9;

    do {
        x = x + 1;
    } while (x = x + 1, x < 4);
    if (x != 4)
        return 10;

    return 42;
}

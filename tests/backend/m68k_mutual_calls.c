int odd(int n);

int even(int n) {
    if (n == 0)
        return 1;

    return odd(n - 1);
}

int odd(int n) {
    if (n == 0)
        return 0;

    return even(n - 1);
}

int main(void) {
    if (!even(10))
        return 1;

    if (!odd(9))
        return 2;

    return 0;
}

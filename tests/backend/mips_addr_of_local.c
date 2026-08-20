int main(void) {
    int x;
    int *p;

    x = 42;
    p = &x;

    if (*p != 42)
        return 1;

    *p = 17;

    if (x != 17)
        return 2;

    return 0;
}

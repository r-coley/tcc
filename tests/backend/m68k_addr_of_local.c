int main(void) {
    int x;
    int *p;

    x = 42;
    p = &x;

    if (*p != 42)
        return 1;

    return 0;
}

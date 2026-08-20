int main(void) {
    int x;
    int *p;

    x = 40;
    p = &x;
    *p = *p + 2;

    if (x != 42)
        return 1;

    return 0;
}

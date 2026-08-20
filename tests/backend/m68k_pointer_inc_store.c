int main(void) {
    int x;
    int y;
    int *p;

    x = 10;
    p = &x;

    y = (*p)++;

    if (y != 10)
        return 1;

    if (x != 11)
        return 2;

    ++(*p);

    if (x != 12)
        return 3;

    return 0;
}

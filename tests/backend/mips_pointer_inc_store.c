int main(void) {
    int x;
    int *p;

    x = 40;
    p = &x;

    ++*p;

    if (x != 41)
        return 1;

    (*p)++;

    if (x != 42)
        return 2;

    return 0;
}

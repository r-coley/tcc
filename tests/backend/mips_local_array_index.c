int main(void) {
    int a[4];

    a[0] = 3;
    a[1] = 5;
    a[2] = 7;
    a[3] = 11;

    if (a[2] != 7)
        return 1;

    if (a[1] + a[3] != 16)
        return 2;

    return 0;
}

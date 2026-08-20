int main(void) {
    int a;
    int b;

    a = 10;
    b = a++;

    if (b != 10)
        return 1;
    if (a != 11)
        return 2;

    b = ++a;

    if (b != 12)
        return 3;
    if (a != 12)
        return 4;

    return 0;
}

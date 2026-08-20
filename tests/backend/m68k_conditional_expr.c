int main(void) {
    int a;
    int b;
    int c;

    a = 1;
    b = 40;
    c = a ? b + 2 : b - 2;

    if (c != 42)
        return 1;

    a = 0;
    c = a ? b + 2 : b - 2;

    if (c != 38)
        return 2;

    return 0;
}

int main(void) {
    char c;
    unsigned char uc;
    short s;
    unsigned short us;
    int i;

    c = -5;
    uc = 250;
    s = -300;
    us = 60000;
    i = 42;

    if (c != -5)
        return 1;
    if (uc != 250)
        return 2;
    if (s != -300)
        return 3;
    if (us != 60000)
        return 4;
    if (i != 42)
        return 5;

    return 0;
}

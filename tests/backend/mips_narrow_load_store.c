int main(void) {
    char c;
    unsigned char uc;
    short s;
    unsigned short us;

    c = -3;
    uc = 250;
    s = -1234;
    us = 50000;

    if (c != -3)
        return 1;
    if (uc != 250)
        return 2;
    if (s != -1234)
        return 3;
    if (us != 50000)
        return 4;

    return 0;
}

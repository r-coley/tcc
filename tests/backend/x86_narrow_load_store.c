struct S {
    char c;
    unsigned char uc;
    short s;
    unsigned short us;
};

int main(void) {
    struct S x;
    x.c = -1;
    x.uc = 250;
    x.s = -2;
    x.us = 1000;

    return (x.c < 0) + (x.uc == 250) + (x.s < 0) + (x.us == 1000);
}

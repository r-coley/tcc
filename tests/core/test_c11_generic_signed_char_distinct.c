int main(void) {
    char c = 0;
    signed char sc = 0;
    unsigned char uc = 0;
    int total = 0;

    total += _Generic(c, char: 10, signed char: 1, unsigned char: 2, default: 100);
    total += _Generic(sc, char: 1, signed char: 20, unsigned char: 2, default: 100);
    total += _Generic(uc, char: 1, signed char: 2, unsigned char: 30, default: 100);

    return total == 60 ? 42 : total;
}

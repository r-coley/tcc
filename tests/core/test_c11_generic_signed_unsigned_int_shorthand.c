int main(void) {
    signed s = 0;
    unsigned u = 0;
    int i = 0;
    unsigned int ui = 0;
    int total = 0;

    total += _Generic(s, signed: 10, unsigned: 1, default: 100);
    total += _Generic(u, signed: 1, unsigned: 20, default: 100);
    total += _Generic(i, signed: 30, unsigned: 1, default: 100);
    total += _Generic(ui, signed: 1, unsigned: 40, default: 100);

    return total == 100 ? 42 : total;
}

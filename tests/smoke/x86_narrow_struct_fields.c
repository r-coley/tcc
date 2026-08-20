struct Mixed {
    char c;
    short s;
    int i;
};

int main(void) {
    struct Mixed m;
    m.c = 5;
    m.s = 1000;
    m.i = 37;
    return m.c + m.s + m.i;
}

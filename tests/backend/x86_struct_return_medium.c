struct Medium {
    int a;
    int b;
    int c;
};

struct Medium make_medium(int x, int y, int z) {
    struct Medium m;

    m.a = x;
    m.b = y;
    m.c = z;

    return m;
}

int consume_medium(struct Medium m) {
    return m.a + m.b * 10 + m.c * 100;
}

int main(void) {
    struct Medium m;
    struct Medium n;

    m = make_medium(1, 2, 3);
    n = make_medium(4, 5, 6);

    if (consume_medium(m) != 321)
        return 1;
    if (consume_medium(n) != 654)
        return 2;

    return 0;
}

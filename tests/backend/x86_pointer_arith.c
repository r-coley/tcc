struct Pair {
    int a;
    int b;
};

int main(void) {
    int xs[4];
    struct Pair p;

    xs[0] = 10;
    xs[1] = 20;
    xs[2] = 12;

    p.a = 30;
    p.b = 12;

    return *(xs + 1) + xs[2] + p.b - 2;
}

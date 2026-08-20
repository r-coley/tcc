struct Pair {
    int a;
    int b;
};

int main(void) {
    struct Pair p;

    p.a = 17;
    p.b = 25;

    if (p.a + p.b != 42)
        return 1;

    return 0;
}

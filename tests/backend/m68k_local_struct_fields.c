struct Pair {
    int a;
    int b;
};

int main(void) {
    struct Pair p;

    p.a = 19;
    p.b = 23;

    if (p.a + p.b != 42)
        return 1;

    return 0;
}

struct Pair {
    int a;
    int b;
};

int main(void) {
    struct Pair p;
    struct Pair *q;

    p.a = 10;
    p.b = 20;
    q = &p;

    q->b = q->a + 32;

    if (p.b != 42)
        return 1;

    return 0;
}

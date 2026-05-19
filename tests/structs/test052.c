struct Pair {
    int a;
    int b;
};

int sum(struct Pair *p) {
    return p->a + p->b;
}

int main() {
    struct Pair x;
    struct Pair y;

    x.a = 50;
    x.b = 57;

    y = x;

    return sum(&y);
}

struct Pair {
    int a;
    int b;
};

int sum(struct Pair p) {
    return p.a + p.b;
}

int main() {
    struct Pair x;

    x.a = 10;
    x.b = 20;

    return sum(x);
}

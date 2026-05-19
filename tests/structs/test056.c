struct Pair {
    int a;
    int b;
};

struct Pair make_pair() {
    struct Pair x;

    x.a = 10;
    x.b = 20;

    return x;
}

int main() {
    struct Pair y;

    y = make_pair();

    return y.a + y.b;
}

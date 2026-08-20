struct Pair {
    int a;
    int b;
};

int main() {
    struct Pair x;
    struct Pair y;

    x.a = 10;
    x.b = 20;

    y = x;

    return y.a + y.b;
}

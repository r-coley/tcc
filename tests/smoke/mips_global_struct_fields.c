struct Pair {
    int a;
    int b;
};

struct Pair g;

int main(void) {
    g.a = 17;
    g.b = 25;

    return g.a + g.b;
}

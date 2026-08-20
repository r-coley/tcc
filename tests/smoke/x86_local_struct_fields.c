struct Pair {
    int a;
    int b;
};

int main(void) {
    struct Pair p;
    p.a = 17;
    p.b = 25;
    return p.a + p.b;
}

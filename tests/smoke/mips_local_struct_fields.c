struct Pair {
    int a;
    int b;
};

int main(void) {
    struct Pair p;
    p.a = 19;
    p.b = 23;

    return p.a + p.b;
}

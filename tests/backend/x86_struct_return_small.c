struct Pair {
    int a;
    int b;
};

struct Pair make_pair(int a, int b) {
    struct Pair p;
    p.a = a;
    p.b = b;
    return p;
}

int main(void) {
    struct Pair p;
    p = make_pair(10, 32);
    return p.a + p.b;
}

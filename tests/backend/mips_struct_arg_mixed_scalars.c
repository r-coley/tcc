struct Pair {
    int a;
    int b;
};

int mix(int x, struct Pair p, int y, int z) {
    return x + p.a + p.b + y + z;
}

int main(void) {
    struct Pair p;

    p.a = 10;
    p.b = 20;

    return mix(1, p, 5, 6);   /* 42 */
}

struct Pair {
    int a;
    int b;
};

int sum_pair(struct Pair p) {
    return p.a + p.b;
}

int add_pair_and_int(int x, struct Pair p, int y) {
    return x + p.a + p.b + y;
}

int main(void) {
    struct Pair p;
    int r1;
    int r2;

    p.a = 10;
    p.b = 20;

    r1 = sum_pair(p);               /* 30 */
    r2 = add_pair_and_int(5, p, 7);  /* 42 */

    return r1 + r2;                 /* 72 */
}

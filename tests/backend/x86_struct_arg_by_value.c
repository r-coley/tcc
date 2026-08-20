struct Pair {
    int a;
    int b;
};

int sum_pair(struct Pair p) {
    return p.a + p.b;
}

int main(void) {
    struct Pair p;
    p.a = 11;
    p.b = 31;
    return sum_pair(p);
}

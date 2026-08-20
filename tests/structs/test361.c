struct Pair {
    int a;
    int b;
};

int sum_pair(struct Pair p) {
    return p.a + p.b;
}

int main() {
    struct Pair p = { .a = 1, .b = 41 };
    return sum_pair(p);
}

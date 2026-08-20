struct pair {
    int a;
    int b;
};

int sum_pair(struct pair p) {
    return p.a + p.b;
}

int main(void) {
    struct pair p;
    p.a = 19;
    p.b = 23;
    return sum_pair(p);
}

struct pair {
    int a;
    int b;
};

int sum(struct pair *p) {
    return p->a + p->b;
}

int main(void) {
    struct pair p;
    p.a = 17;
    p.b = 25;
    return sum(&p);
}

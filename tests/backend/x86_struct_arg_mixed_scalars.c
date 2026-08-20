struct Small {
    int a;
    int b;
};

int use_mixed(int x, struct Small s, int y, long long z) {
    return x + s.a + s.b + y + (int)z;
}

int call_mixed(void) {
    struct Small s;

    s.a = 20;
    s.b = 300;

    return use_mixed(1, s, 4000, 50000LL);
}

int use_mixed_twice(struct Small a, int x, struct Small b) {
    return a.a + a.b + x + b.a + b.b;
}

int call_mixed_twice(void) {
    struct Small a;
    struct Small b;

    a.a = 1;
    a.b = 2;
    b.a = 30;
    b.b = 400;

    return use_mixed_twice(a, 5000, b);
}

int main(void) {
    if (call_mixed() != 54321)
        return 1;
    if (call_mixed_twice() != 5433)
        return 2;
    return 0;
}

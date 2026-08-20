struct Ops {
    int (*fn)(int, int);
};

int add(int a, int b) {
    return a + b;
}

int main(void) {
    struct Ops ops;
    ops.fn = add;
    return ops.fn(20, 22);
}

struct Ops {
    int (*fn)(int);
};

int add5(int x) {
    return x + 5;
}

int add7(int x) {
    return x + 7;
}

int call_op(struct Ops *ops, int x) {
    return ops->fn(x);
}

int main(void) {
    struct Ops ops;
    int r1;
    int r2;

    ops.fn = add5;
    r1 = call_op(&ops, 30);

    ops.fn = add7;
    r2 = call_op(&ops, 0);

    return r1 + r2;    /* 35 + 7 = 42 */
}

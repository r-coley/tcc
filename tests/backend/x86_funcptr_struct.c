struct Op {
    int base;
    int (*fn)(int, int);
};

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main(void) {
    struct Op op;
    op.base = 40;
    op.fn = add;
    return op.fn(op.base, 2) + sub(3, 3);
}

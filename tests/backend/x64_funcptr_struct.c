struct ops {
    int (*fn)(int, int);
};

int add(int a, int b) {
    return a + b;
}

int main(void) {
    struct ops o;
    o.fn = add;
    return o.fn(19, 23);
}

int add1(int x) {
    return x + 1;
}

int add2(int x) {
    return x + 2;
}

int main(void) {
    int (*fn)(int);

    fn = add1;
    if (fn(40) != 41)
        return 1;

    fn = add2;
    if (fn(40) != 42)
        return 2;

    return 0;
}

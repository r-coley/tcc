int add1(int x) {
    return x + 1;
}

int add2(int x) {
    return x + 2;
}

int main(void) {
    int (*fp)(int);
    int r;

    fp = add1;
    r = fp(10);

    fp = add2;
    r = r + fp(30);

    return r;
}

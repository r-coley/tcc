int add1(int x) {
    return x + 1;
}

int add2(int x) {
    return x + 2;
}

int main(void) {
    int (*fp)(int);
    int x;

    fp = add1;
    x = fp(40);

    fp = add2;
    x = fp(x);

    if (x != 43)
        return 1;

    return 0;
}

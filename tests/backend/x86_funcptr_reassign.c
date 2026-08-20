int add1(int x) { return x + 1; }
int add10(int x) { return x + 10; }
int main(void) {
    int (*fp)(int);
    int r;
    fp = add1;
    r = fp(1);
    fp = add10;
    return r + fp(30);
}

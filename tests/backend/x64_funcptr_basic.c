int plus2(int x) {
    return x + 2;
}

int main(void) {
    int (*fp)(int);
    fp = plus2;
    return fp(40);
}

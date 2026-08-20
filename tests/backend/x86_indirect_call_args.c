int sum5(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}

int main(void) {
    int (*fp)(int, int, int, int, int);
    fp = sum5;
    return fp(1, 2, 3, 4, 32);
}

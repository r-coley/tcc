int sum3(int a, int b, int c) {
    return a + b + c;
}

int main(void) {
    int (*fp)(int, int, int);

    fp = sum3;

    if (fp(10, 20, 12) != 42)
        return 1;

    return 0;
}

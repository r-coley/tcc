int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int main(void) {
    if (add(mul(6, 7), add(10, -10)) != 42)
        return 1;

    return 0;
}

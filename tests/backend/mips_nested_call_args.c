int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int main(void) {
    if (add(mul(2, 3), add(4, 5)) != 15)
        return 1;
    return 0;
}

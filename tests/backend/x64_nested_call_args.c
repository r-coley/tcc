int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int main(void) {
    return add(mul(3, 4), add(10, 20));
}

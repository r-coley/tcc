int add5(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}

int mix(int a, int b, int c) {
    return a - b + c;
}

int main(void) {
    return add5(1, 2, 3, 4, 32) + mix(20, 10, 0);
}

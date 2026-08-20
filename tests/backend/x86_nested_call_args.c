int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int mix(int a, int b, int c) { return a + b - c; }

int main(void) {
    return mix(add(10, 20), mul(3, 4), add(5, 6)) - 31;
}

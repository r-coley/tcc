#define CALL(fn, ...) fn(20, ##__VA_ARGS__)

int add3(int a, int b, int c) {
    return a + b + c;
}

int main() {
    return CALL(add3, 10, 12);
}

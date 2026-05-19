#define CALL(fn, ...) fn(40, ##__VA_ARGS__)

int add(int a, int b) {
    return a + b;
}

int main() {
    return CALL(add, 2);
}

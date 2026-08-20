#define CALL(fn, ...) fn(42, ##__VA_ARGS__)

int one(int a) {
    return a;
}

int main() {
    return CALL(one);
}

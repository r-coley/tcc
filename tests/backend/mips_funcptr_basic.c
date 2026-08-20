int add3(int x) {
    return x + 3;
}

int call_it(int (*fn)(int), int value) {
    return fn(value);
}

int main(void) {
    if (call_it(add3, 39) != 42)
        return 1;
    return 0;
}

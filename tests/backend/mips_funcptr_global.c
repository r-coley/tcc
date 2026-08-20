int add3(int x) {
    return x + 3;
}

int (*global_fn)(int) = add3;

int main(void) {
    if (global_fn(39) != 42)
        return 1;
    return 0;
}

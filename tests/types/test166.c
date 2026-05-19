int forty_two(void) {
    return 42;
}

int main() {
    int (*fp)(void) = forty_two;
    return fp();
}

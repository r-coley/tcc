int set_to_42(int *p) {
    *p = 42;
    return 0;
}

int main() {
    int a;

    a = 0;
    set_to_42(&a);

    return a;
}

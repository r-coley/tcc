int main() {
    int a;
    int *p;

    a = 0;
    p = &a;
    *p = 42;

    return a;
}

int main() {
    int x;
    int *p;

    x = 0;
    p = (int *)&x;
    *p = 42;

    return x;
}

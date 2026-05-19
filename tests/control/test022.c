int main() {
    int a[4];
    int *p;

    a[0] = 11;
    a[1] = 22;
    a[2] = 33;
    a[3] = 44;

    p = a;
    *(p + 1) = 55;

    return a[1];
}

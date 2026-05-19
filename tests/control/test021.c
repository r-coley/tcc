int main() {
    int a[4];
    int *p;

    a[0] = 11;
    a[1] = 22;
    a[2] = 99;
    a[3] = 44;

    p = a;

    return *(p + 2);
}

int main() {
    int a[3];
    int *p;

    a[0] = 11;
    a[1] = 55;
    a[2] = 99;

    p = &a[1];

    return *p;
}

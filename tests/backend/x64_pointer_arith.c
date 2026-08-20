int main(void) {
    int a[4];
    int *p;
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    a[3] = 40;
    p = a;
    return *(p + 2);
}

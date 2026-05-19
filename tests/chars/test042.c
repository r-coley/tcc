int main() {
    char a[4];
    char *p;

    a[0] = 65;
    a[1] = 66;
    a[2] = 67;
    a[3] = 68;

    p = a;

    return *(p + 2);
}

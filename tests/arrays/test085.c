int main() {
    int a[3] = {4, 5, 6};
    int *p;

    p = a;

    return *(p + 2);
}

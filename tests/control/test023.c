int main() {
    int a[5];
    int *p;
    int i;
    int total;

    i = 0;
    while (i < 5) {
        a[i] = i + 1;
        i = i + 1;
    }

    p = a;
    i = 0;
    total = 0;

    while (i < 5) {
        total = total + *(p + i);
        i = i + 1;
    }

    return total;
}

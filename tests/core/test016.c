int main() {
    int a[5];
    int i;
    int total;

    i = 0;

    while (i < 5) {
        a[i] = i;
        i = i + 1;
    }

    i = 0;
    total = 0;

    while (i < 5) {
        total = total + a[i];
        i = i + 1;
    }

    return total;
}

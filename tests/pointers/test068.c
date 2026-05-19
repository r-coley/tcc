int sum(int *p) {
    return *p + *(p + 1);
}

int main() {
    int a[2];

    a[0] = 12;
    a[1] = 30;

    return sum(a);
}

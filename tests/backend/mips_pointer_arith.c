int read_index(int *p, int idx) {
    return p[idx];
}

int main(void) {
    int a[3];

    a[0] = 11;
    a[1] = 22;
    a[2] = 33;

    if (read_index(a, 2) != 33)
        return 1;
    return 0;
}

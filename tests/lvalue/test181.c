int main() {
    int x;
    int *p;

    x = 4;
    p = &x;
    *p += 2;

    return x;
}

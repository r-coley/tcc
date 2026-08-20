int main(void) {
    int x = 0;
    int *ip = &x;
    const int *cip = &x;
    char *cp = 0;
    int a[3];
    int total = 0;

    total += _Generic(ip, char *: 1, int *: 10, default: 100);
    total += _Generic(cip, int *: 1, const int *: 20, default: 100);
    total += _Generic(cp, int *: 1, char *: 30, default: 100);
    total += _Generic(a, char *: 1, int *: 40, default: 100);

    return total == 100 ? 42 : total;
}

int main(void) {
    int x = 0;
    void *vp = &x;
    int *ip = &x;
    int total = 0;

    total += _Generic(vp, int *: 1, void *: 10, default: 100);
    total += _Generic(ip, int *: 20, void *: 1, default: 100);

    return total == 30 ? 42 : total;
}

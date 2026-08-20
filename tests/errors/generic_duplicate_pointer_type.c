int main(void) {
    int x;
    int *p = &x;
    return _Generic(p, int *: 1, int *: 2, default: 3);
}

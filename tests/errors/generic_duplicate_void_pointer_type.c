int main(void) {
    int x;
    void *p = &x;
    return _Generic(p, void *: 1, void *: 2, default: 3);
}

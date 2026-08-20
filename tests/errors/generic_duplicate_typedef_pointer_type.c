typedef int *ip;

int main(void) {
    int x;
    int *p = &x;
    return _Generic(p, int *: 1, ip: 2, default: 3);
}

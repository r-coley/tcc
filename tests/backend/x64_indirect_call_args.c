int mix(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

int main(void) {
    int (*fp)(int, int, int, int, int, int, int);
    fp = mix;
    return fp(1, 2, 3, 4, 5, 6, 7);
}

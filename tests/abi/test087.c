int mix8(int a, int b, int c, int d, int e, int f, int g, int h) {
    return h - g + f - e + d - c + b - a;
}

int main() {
    return mix8(1, 3, 5, 7, 11, 13, 17, 19);
}

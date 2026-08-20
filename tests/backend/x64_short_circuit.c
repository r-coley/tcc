int f(int a, int b, int c) {
    if (a && b)
        return 11;
    if (a || c)
        return 22;
    return 33;
}

int main(void) {
    return f(0, 1, 1);
}

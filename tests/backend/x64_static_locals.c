int f(void) {
    static int x = 40;
    x = x + 1;
    return x;
}

int main(void) {
    f();
    return f();
}

int f() {
    static int x = 40;
    x = x + 1;
    return x;
}

int main() {
    f();
    return f();
}

int f() {
    static int x;
    x = x + 42;
    return x;
}

int main() {
    return f();
}

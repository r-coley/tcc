int f() {
    static int a[2];
    a[1] = 42;
    return a[1];
}

int main() {
    return f();
}

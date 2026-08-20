int f() {
    static int a[2] = { 40, 2 };
    return a[0] + a[1];
}

int main() {
    return f();
}

int f() {
    static char x = 40;
    x = x + 2;
    return x;
}

int main() {
    return f();
}

int f() {
    static char s[] = "ABC";
    return s[0] - 23;
}

int main() {
    return f();
}

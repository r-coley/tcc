int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int main() {
    int (*fp)(int, int);
    fp = sub;
    fp = add;
    return fp(20, 22);
}

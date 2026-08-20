int f(int a, int b) {
    int q;
    int r;
    q = a / b;
    r = a % b;
    return q * 10 + r;
}

int main(void) {
    return f(-43, 5);
}

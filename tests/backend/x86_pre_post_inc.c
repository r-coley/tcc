int main(void) {
    int x;
    int a;
    int b;

    x = 10;
    a = x++;
    b = ++x;

    return a + b + x;
}

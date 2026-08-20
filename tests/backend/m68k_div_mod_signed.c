int main(void) {
    int a;
    int b;

    a = 43;
    b = 10;

    if (a / b != 4)
        return 1;

    if (a % b != 3)
        return 2;

    return 0;
}

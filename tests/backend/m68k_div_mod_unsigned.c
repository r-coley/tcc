int main(void) {
    unsigned int a;
    unsigned int b;

    a = 43u;
    b = 10u;

    if (a / b != 4u)
        return 1;

    if (a % b != 3u)
        return 2;

    return 0;
}

int add1(int x) {
    return x + 1;
}

int main(void) {
    int (*fp)(int);

    fp = add1;

    if (fp(41) != 42)
        return 1;

    return 0;
}

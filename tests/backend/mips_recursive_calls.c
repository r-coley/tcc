int fact(int n) {
    if (n <= 1)
        return 1;
    return n * fact(n - 1);
}

int main(void) {
    if (fact(5) != 120)
        return 1;
    return 0;
}

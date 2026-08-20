int next(void) {
    static int counter = 40;

    counter = counter + 1;
    return counter;
}

int main(void) {
    if (next() != 41)
        return 1;

    if (next() != 42)
        return 2;

    return 0;
}

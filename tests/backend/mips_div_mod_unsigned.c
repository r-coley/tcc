unsigned int divu_test(unsigned int a, unsigned int b) {
    return a / b;
}

unsigned int modu_test(unsigned int a, unsigned int b) {
    return a % b;
}

int main(void) {
    if (divu_test(100U, 9U) != 11U)
        return 1;
    if (modu_test(100U, 9U) != 1U)
        return 2;
    return 0;
}

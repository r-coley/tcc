int signed_lt(int a, int b) {
    return a < b;
}

int unsigned_gt(unsigned int a, unsigned int b) {
    return a > b;
}

int main(void) {
    if (!signed_lt(-1, 1))
        return 1;
    if (!unsigned_gt(0xffffffffU, 1U))
        return 2;
    return 0;
}

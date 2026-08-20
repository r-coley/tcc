int signed_lt(int a, int b) {
    if (a < b)
        return 11;
    return 22;
}

int unsigned_lt(unsigned a, unsigned b) {
    if (a < b)
        return 33;
    return 44;
}

int main(void) {
    return signed_lt(-1, 1) + unsigned_lt(1u, 2u);
}

int signed_lt(int a, int b) {
    return a < b;
}

int unsigned_gt(unsigned a, unsigned b) {
    return a > b;
}

int main(void) {
    return signed_lt(-1, 1) + unsigned_gt(4000000000u, 1u);
}

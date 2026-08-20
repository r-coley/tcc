unsigned f(unsigned a, unsigned b) {
    unsigned q;
    unsigned r;
    q = a / b;
    r = a % b;
    return q * 10u + r;
}

int main(void) {
    return (int)f(43u, 5u);
}

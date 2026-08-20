int main(void) {
    unsigned u = 0;
    return _Generic(u, unsigned: 1, unsigned int: 2, default: 3);
}

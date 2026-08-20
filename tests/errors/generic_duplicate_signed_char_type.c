int main(void) {
    signed char c = 0;
    return _Generic(c, signed char: 1, signed char: 2, default: 3);
}

int main(void) {
    signed s = 0;
    return _Generic(s, signed: 1, int: 2, default: 3);
}

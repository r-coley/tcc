int main() {
    int x = 42;
    goto skip;
    x = 99;

skip:
    return x;
}

int main() {
    int x = 1;
    goto skip;
    x = 99;

skip:
    return x;
}

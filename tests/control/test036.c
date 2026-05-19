int main() {
    int x = 0;

loop:
    x = x + 1;
    if (x < 42)
        goto loop;

    return x;
}

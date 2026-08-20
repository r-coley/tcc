int main() {
    int x = 0;

a:
    x = x + 1;
    if (x == 10)
        goto b;
    goto a;

b:
    return x + 32;
}

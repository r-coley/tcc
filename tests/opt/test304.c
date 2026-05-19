int main() {
    int x;

    asm("nop");
    x = 41;
    x++;

    return x;
}

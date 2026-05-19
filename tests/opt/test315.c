int main() {
    int x;

    x = 40;
    asm volatile ("nop");

    return x + 2;
}

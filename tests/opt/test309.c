int main() {
    int x;

    x = 41;
    asm volatile ("nop");
    x = x + 1;

    return x;
}

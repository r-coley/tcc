int main() {
    int x;

    x = 1;
    asm volatile ("nop");
    x = 42;

    return x;
}

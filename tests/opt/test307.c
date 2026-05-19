int main() {
    asm("nop" : : : "memory");
    return 42;
}

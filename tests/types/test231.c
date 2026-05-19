/* typedef long used as local variable - must allocate 2 slots (8 bytes) */
typedef long off_t;

int main() {
    off_t x = 42;
    return (int)x;
}

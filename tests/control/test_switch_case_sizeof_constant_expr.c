int main(void) {
    switch (sizeof(int)) {
    case sizeof(int):
        return 42;
    default:
        return 1;
    }
}

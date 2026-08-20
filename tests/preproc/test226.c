#define VA(...) \
    __VA_ARGS__

int main() {
    return VA(6 * 7);
}

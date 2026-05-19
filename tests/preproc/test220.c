#define FIRST_PLUS(base, ...) ((base)+(__VA_ARGS__))

int main() {
    return FIRST_PLUS(1, 2 + 39);
}

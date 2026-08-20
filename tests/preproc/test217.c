#define ADDV(base, ...) ((base)+(__VA_ARGS__))

int main() {
    return ADDV(40, 2);
}

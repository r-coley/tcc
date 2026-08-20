#define PICK(fmt, ...) fmt ##__VA_ARGS__

int main() {
    return PICK(42);
}

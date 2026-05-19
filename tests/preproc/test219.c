#define X 6
#define VA(...) __VA_ARGS__

int main() {
    return VA(X * 7);
}

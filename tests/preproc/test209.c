#define MAKE(n) value ## n

int main() {
    int value1;

    value1 = 77;

    return MAKE(1);
}

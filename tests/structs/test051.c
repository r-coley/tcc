struct Pair {
    char c;
    int value;
};

int main() {
    struct Pair a;
    struct Pair b;

    a.c = 101;
    a.value = 7;

    b = a;

    return b.c + b.value;
}

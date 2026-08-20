struct Box {
    int value;
    int other;
};

int main() {
    struct Box b;

    b.value = 42;
    b.other = 99;

    return b.value;
}

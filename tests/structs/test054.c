struct Box {
    char c;
    int value;
};

int get(struct Box b) {
    return b.c + b.value;
}

int main() {
    struct Box x;

    x.c = 35;
    x.value = 7;

    return get(x);
}

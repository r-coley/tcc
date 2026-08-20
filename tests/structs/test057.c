struct Box {
    char c;
    int value;
};

struct Box make_box() {
    struct Box x;

    x.c = 35;
    x.value = 7;

    return x;
}

int main() {
    struct Box b;

    b = make_box();

    return b.c + b.value;
}

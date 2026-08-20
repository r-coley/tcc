struct Box {
    int value;
};

int main() {
    struct Box b;
    struct Box *p;

    p = &b;
    p->value = 42;

    return p->value;
}

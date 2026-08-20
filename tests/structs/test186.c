struct Inner {
    char c;
    int i;
};

struct Outer {
    char tag;
    struct Inner inner;
};

int main() {
    return sizeof(struct Outer);
}

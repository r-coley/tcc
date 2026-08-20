struct Small {
    char c;
};

struct Outer {
    char a;
    struct Small s;
    int i;
};

int main() {
    return sizeof(struct Outer);
}

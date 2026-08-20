struct Mixed {
    char c;
    int n;
};

int main() {
    struct Mixed a = { .c = 2, .n = 40 };
    struct Mixed b;

    b = a;

    return b.c + b.n;
}

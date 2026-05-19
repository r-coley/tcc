struct Mixed {
    char c;
    int n;
};

struct Mixed make_mixed() {
    struct Mixed m = { .c = 2, .n = 40 };
    return m;
}

int main() {
    struct Mixed m;
    m = make_mixed();
    return m.c + m.n;
}

struct Mixed {
    char c;
    int n;
};

int mutate_mixed(struct Mixed m) {
    m.c = 100;
    return m.c;
}

int main() {
    struct Mixed m = { .c = 2, .n = 40 };
    mutate_mixed(m);
    return m.c + m.n;
}

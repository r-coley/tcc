struct Mixed {
    char c;
    int n;
};

int sum_mixed(struct Mixed m) {
    return m.c + m.n;
}

int main() {
    struct Mixed m = { .c = 2, .n = 40 };
    return sum_mixed(m);
}

struct Mixed {
    char c;
    int n;
};

int main() {
    struct Mixed m = { .n = 42 };

    return m.c + m.n;
}

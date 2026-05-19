struct Mixed {
    char c;
    int n;
};

int main() {
    struct Mixed m = { .n = 40, .c = 2 };

    return m.c + m.n;
}

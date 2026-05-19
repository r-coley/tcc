struct Pair {
    int a;
    int b;
};

int main() {
    struct Pair p;
    int y;

    p.b = 4;
    y = ++p.b;

    return y * 10 + p.b;
}

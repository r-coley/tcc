struct Pair {
    int a;
    int b;
};

int main() {
    struct Pair p;
    struct Pair *q;
    int y;

    p.b = 8;
    q = &p;
    y = --q->b;

    return y * 10 + p.b;
}

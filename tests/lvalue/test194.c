struct Pair {
    int a;
    int b;
};

int main() {
    struct Pair p;
    struct Pair *q;
    int y;

    p.a = 6;
    q = &p;
    y = q->a++;

    return y * 10 + p.a;
}

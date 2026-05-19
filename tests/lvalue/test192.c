struct Pair {
    int a;
    int b;
};

int main() {
    struct Pair p;
    int y;

    p.a = 3;
    y = p.a++;

    return y * 10 + p.a;
}

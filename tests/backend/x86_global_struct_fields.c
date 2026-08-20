struct Pair {
    int a;
    char c;
    short s;
    int b;
};

struct Pair gp = { 10, 3, 5, 24 };

int main(void) {
    return gp.a + gp.c + gp.s + gp.b;
}

struct Pair {
    int a;
    int b;
};

struct Pair gp = { 19, 23 };

int main(void) {
    if (gp.a + gp.b != 42)
        return 1;

    return 0;
}

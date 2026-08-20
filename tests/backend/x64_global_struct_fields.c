struct rec {
    char c;
    short s;
    int i;
};

struct rec g = { 7, 300, 4000 };

int main(void) {
    return g.c + g.s + g.i;
}

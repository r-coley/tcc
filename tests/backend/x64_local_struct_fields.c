struct rec {
    char c;
    short s;
    int i;
};

int main(void) {
    struct rec r;
    r.c = 7;
    r.s = 300;
    r.i = 4000;
    return r.c + r.s + r.i;
}

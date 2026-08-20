struct Rec {
    char c;
    short s;
    int i;
};

int main(void) {
    struct Rec r;

    r.c = 10;
    r.s = 20;
    r.i = 12;

    if (r.c + r.s + r.i != 42)
        return 1;

    return 0;
}

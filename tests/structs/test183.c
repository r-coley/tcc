struct S {
    char c;
    int i;
};

int main() {
    struct S s;
    s.c = 1;
    s.i = 41;
    return s.c + s.i;
}

struct S {
    char c;
    int i;
};

struct S s = { .i = 41, .c = 1 };

int main() {
    return s.c + s.i;
}

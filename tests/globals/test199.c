struct S {
    char c;
    int i;
};

struct S s = { 1, 41 };

int main() {
    return s.c + s.i;
}

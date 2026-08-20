struct S {
    int a;
    int b;
};

struct S s = { .b = 2 };

int main(void) {
    return s.b;
}

struct Inner {
    char c;
    int n;
};

struct Outer {
    int a;
    struct Inner i;
};

int main() {
    struct Outer o = { .a = 1, .i = { .n = 40, .c = 1 } };

    return o.a + o.i.c + o.i.n;
}

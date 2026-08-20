struct Inner {
    int a;
    int b;
};

struct Middle {
    struct Inner inner;
    int c;
};

struct Outer {
    int z;
    struct Middle mid;
};

int main() {
    struct Outer o = { .z = 1, .mid = { .inner = { 40, 2 }, .c = 9 } };
    struct Outer *p;
    p = &o;

    return p->mid.inner.a + p->mid.inner.b;
}

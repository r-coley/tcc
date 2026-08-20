struct Inner {
    int x;
    int y;
};

struct Outer {
    int a;
    struct Inner inner;
    int b;
};

int main(void) {
    struct Outer o;
    o.a = 5;
    o.inner.x = 11;
    o.inner.y = 17;
    o.b = 9;
    return o.a + o.inner.x + o.inner.y + o.b;
}

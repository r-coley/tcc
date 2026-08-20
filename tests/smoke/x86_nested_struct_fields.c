struct Inner {
    int x;
    int y;
};

struct Outer {
    int tag;
    struct Inner inner;
};

int main(void) {
    struct Outer o;
    o.tag = 5;
    o.inner.x = 11;
    o.inner.y = 26;
    return o.tag + o.inner.x + o.inner.y;
}

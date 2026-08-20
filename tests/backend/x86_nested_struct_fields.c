struct Inner {
    char c;
    short s;
    int i;
};

struct Outer {
    int left;
    struct Inner inner;
    int right;
};

static struct Outer global_outer = {
    10,
    { 3, 20, 300 },
    4000
};

int read_global_nested(void) {
    return global_outer.left
         + global_outer.inner.c
         + global_outer.inner.s
         + global_outer.inner.i
         + global_outer.right;
}

int read_local_nested(void) {
    struct Outer o;

    o.left = 1;
    o.inner.c = 2;
    o.inner.s = 30;
    o.inner.i = 400;
    o.right = 5000;

    return o.left + o.inner.c + o.inner.s + o.inner.i + o.right;
}

int main(void) {
    if (read_global_nested() != 4333)
        return 1;
    if (read_local_nested() != 5433)
        return 2;
    return 0;
}

struct Inner {
    int x;
    int y;
};

struct Outer {
    int a;
    struct Inner in;
    int b;
};

int main(void) {
    struct Outer o;

    o.a = 10;
    o.in.x = 11;
    o.in.y = 12;
    o.b = 9;

    if (o.a + o.in.x + o.in.y + o.b != 42)
        return 1;

    return 0;
}

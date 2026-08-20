struct Inner {
    int a;
    int b;
};

struct Outer {
    int x;
    struct Inner in;
    int y;
};

int main(void) {
    struct Outer o;

    o.x = 5;
    o.in.a = 10;
    o.in.b = 20;
    o.y = 7;

    return o.x + o.in.a + o.in.b + o.y;  /* 42 */
}

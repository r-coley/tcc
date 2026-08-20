struct Quad {
    int a;
    int b;
    int c;
    int d;
};

struct Quad make_quad(int a, int b, int c, int d) {
    struct Quad q;

    q.a = a;
    q.b = b;
    q.c = c;
    q.d = d;

    return q;
}

int main(void) {
    struct Quad q;

    q = make_quad(5, 10, 20, 7);

    return q.a + q.b + q.c + q.d;   /* 42 */
}

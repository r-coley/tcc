struct Node {
    int a;
    int b;
};

int main(void) {
    struct Node n;
    struct Node *p;

    n.a = 11;
    n.b = 31;
    p = &n;

    return p->a + p->b;
}

struct pair {
    int a;
    int b;
};

struct pair make_pair(int a, int b) {
    struct pair p;
    p.a = a;
    p.b = b;
    return p;
}

int main(void) {
    struct pair p;
    p = make_pair(19, 23);
    return p.a + p.b;
}

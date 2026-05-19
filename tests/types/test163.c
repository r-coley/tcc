struct Point {
    int x;
    int y;
};

union Holder {
    struct Point p;
    int i;
};

int main() {
    union Holder h;
    h.p.x = 42;
    return h.p.x;
}

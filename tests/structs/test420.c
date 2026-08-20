struct Point {
    int x;
    int y;
};

int main() {
    struct Point p = { 1, 2 };
    struct Point *q;
    q = &p;

    q->x = 42;
    return p.x;
}

struct Point {
    int x;
    int y;
};

int main() {
    struct Point p = { 42, 1 };
    struct Point *q;
    q = &p;

    return q->x;
}

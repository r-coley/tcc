struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;
    struct Point *q;

    q = &p;
    q->x = 10;
    q->y = 20;

    return q->x + q->y;
}

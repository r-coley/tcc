struct Point {
    int x;
    int y;
};

int init(struct Point *p) {
    p->x = 40;
    p->y = 2;
    return 0;
}

int main() {
    struct Point p;

    init(&p);

    return p.x + p.y;
}

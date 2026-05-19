struct Point {
    int x;
    int y;
};

int getx(struct Point *p) {
    return p->x;
}

int main() {
    struct Point p;

    p.x = 77;
    p.y = 12;

    return getx(&p);
}

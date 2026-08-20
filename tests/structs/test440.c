struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[3] = {
        { 10, 1 },
        { 20, 2 },
        { 12, 3 }
    };

    struct Point *p;
    int total;

    p = &pts[0];
    total = p->x;

    p = p + 1;
    total = total + p->x;

    p = p + 1;
    total = total + p->x;

    return total;
}

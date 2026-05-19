struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[2] = {
        { 1, 2 },
        { 41, 9 }
    };

    struct Point *p;
    p = &pts[1];
    p = p - 1;

    return p->x + pts[1].x;
}

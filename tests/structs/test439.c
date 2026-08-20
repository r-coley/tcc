struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[2] = {
        { 1, 2 },
        { 42, 9 }
    };

    struct Point *p;
    p = &pts[0];
    p = p + 1;

    return p->x;
}

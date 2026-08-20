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

    return p[1].x;
}

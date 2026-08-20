struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[2] = {
        { 1, 2 },
        { 3, 4 }
    };

    struct Point *p;
    p = &pts[0];

    p[1].y = 41;
    return p[1].x + pts[1].y - 2;
}

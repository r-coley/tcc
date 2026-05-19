struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[2];

    pts[0].x = 42;
    pts[0].y = 1;

    return pts[0].x;
}

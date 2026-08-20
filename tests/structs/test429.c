struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[2];

    pts[0].x = 1;
    pts[1].x = 41;

    return pts[0].x + pts[1].x;
}

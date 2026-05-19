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

    return pts[0].x + pts[1].x + pts[2].x;
}

struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[3] = {
        [2] = { 40, 2 },
        [0] = { 1, 2 }
    };

    return pts[2].x + pts[2].y;
}

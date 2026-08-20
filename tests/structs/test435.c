struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[2] = {
        [1] = { 41, 9 },
        [0] = { 1, 2 }
    };

    return pts[0].x + pts[1].x;
}

struct Point {
    int x;
    int y;
};

int main() {
    struct Point pts[3] = {
        [1] = { 20, 2 },
        { 22, 3 }
    };

    return pts[1].x + pts[2].x;
}

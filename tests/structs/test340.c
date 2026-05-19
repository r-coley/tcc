struct Point {
    int x;
    int y;
};

int main() {
    struct Point p = { 1, .y = 41 };

    return p.x + p.y;
}

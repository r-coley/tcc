struct Point {
    int x;
    int y;
};

int main() {
    struct Point p = { 40 + 1, 1 };

    return p.x + p.y;
}

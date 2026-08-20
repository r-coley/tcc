struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    return p;
}

int main() {
    struct Point p = make_point();
    return p.x + p.y;
}

struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    return p;
}

int combine(int bias, struct Point p) {
    return bias + p.x + p.y;
}

int main() {
    return combine(0, make_point());
}

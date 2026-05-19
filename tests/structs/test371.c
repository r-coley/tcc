struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    return p;
}

int sum_point(struct Point p) {
    return p.x + p.y;
}

int main() {
    return sum_point(make_point());
}

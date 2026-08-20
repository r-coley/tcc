struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 42, 42 };
    return p;
}

int main() {
    if (make_point().x == make_point().y)
        return 42;

    return 0;
}

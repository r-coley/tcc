struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 42, 1 };
    return p;
}

int main() {
    if (make_point().x == 42)
        return 42;

    return 0;
}

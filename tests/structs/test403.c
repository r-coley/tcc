struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    return p;
}

int main() {
    return make_point().x + make_point().y;
}

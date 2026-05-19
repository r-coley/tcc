struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 42, 1 };
    return p;
}

int main() {
    return make_point().x;
}

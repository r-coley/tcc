struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 6, 7 };
    return p;
}

int main() {
    return make_point().x * make_point().y;
}

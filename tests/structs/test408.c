int g;

int sink2(int a, int b) {
    g = a + b;
    return 0;
}

struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    return p;
}

int main() {
    sink2(make_point().x, make_point().y);
    return g;
}

int g;

struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    g = 42;
    return p;
}

int main() {
    make_point();
    return g;
}

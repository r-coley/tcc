int g;

int sink(int x) {
    g = x;
    return 0;
}

struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 42, 1 };
    return p;
}

int main() {
    sink(make_point().x);
    return g;
}

struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 42, 1 };
    return p;
}

struct Point wrapper() {
    struct Point p;
    p = make_point();
    return p;
}

int main() {
    struct Point p = wrapper();
    return p.x;
}

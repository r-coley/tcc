struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 42, 1 };
    return p;
}

struct Point wrapper() {
    return make_point();
}

int main() {
    struct Point p = wrapper();
    return p.x;
}

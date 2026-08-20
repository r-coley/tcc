struct Point {
    int x;
    int y;
};

struct Point make_point() {
    struct Point p = { 1, 41 };
    return p;
}

int main() {
    int z = make_point().y;
    return z + 1;
}

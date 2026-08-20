struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;

    p = (struct Point){ 2, 40 };

    return p.x + p.y;
}

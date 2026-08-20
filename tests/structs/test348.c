struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;

    p = (struct Point){ .y = 42 };

    return p.x + p.y;
}

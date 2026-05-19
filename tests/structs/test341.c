struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;

    p = (struct Point){ .y = 41, .x = 1 };

    return p.x + p.y;
}

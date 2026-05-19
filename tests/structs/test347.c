struct Point {
    int x;
    int y;
};

int main() {
    struct Point p = { .y = 42 };

    return p.x + p.y;
}

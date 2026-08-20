int g;

struct Point {
    int x;
    int y;
};

int sink(struct Point p) {
    g = p.x + p.y;
    return 0;
}

int main() {
    sink((struct Point){ .y = 41, .x = 1 });
    return g;
}

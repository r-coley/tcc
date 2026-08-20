struct Point {
    int x;
    int y;
};

int sum_point(struct Point p) {
    return p.x + p.y;
}

int main() {
    return sum_point((struct Point){ .y = 41, .x = 1 });
}

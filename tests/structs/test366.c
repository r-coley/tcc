struct Point {
    int x;
    int y;
};

int mutate(struct Point p) {
    p.x = 100;
    return p.x;
}

int main() {
    struct Point p = { 1, 41 };
    mutate(p);
    return p.x + p.y;
}

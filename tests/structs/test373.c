int g;

struct Point {
    int x;
    int y;
};

int sink2(struct Point a, struct Point b) {
    g = a.x + a.y + b.x + b.y;
    return 0;
}

int main() {
    sink2((struct Point){ 1, 2 }, (struct Point){ 39, 0 });
    return g;
}

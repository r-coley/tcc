struct Point {
    int x;
    int y;
};

int mix(int bias, struct Point p) {
    return bias + p.x + p.y;
}

int main() {
    return mix(0, (struct Point){ 1, 41 });
}

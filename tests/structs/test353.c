struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 1, 41 };
    struct Point b;

    b = a;

    return b.x + b.y;
}

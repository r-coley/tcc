struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 0, 0 };
    struct Point c = { 1, 41 };

    a = b = c;

    return b.x + b.y;
}

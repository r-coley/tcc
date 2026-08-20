struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 1, 41 };

    return (a = b).x + a.y;
}

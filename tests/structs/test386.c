struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 1, 41 };

    if ((a = b).x == 1)
        return a.x + a.y;

    return 0;
}

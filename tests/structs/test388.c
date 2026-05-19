struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 1, 2 };
    struct Point c = { 0, 0 };
    struct Point d = { 40, 41 };

    return (a = b).x + (c = d).y;
}

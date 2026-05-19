struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 42, 1 };
    struct Point c = { 0, 0 };
    struct Point d = { 2, 42 };

    if ((a = b).x == (c = d).y)
        return 42;

    return 0;
}

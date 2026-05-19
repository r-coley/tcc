struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 42, 5 };

    return (a = b).x;
}

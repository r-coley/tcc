struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 1, 42 };

    return (a = b).y;
}

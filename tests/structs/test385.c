int g;

int sink(int x) {
    g = x;
    return 0;
}

struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 42, 9 };

    sink((a = b).x);
    return g;
}

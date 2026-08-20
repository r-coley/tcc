int g;

int sink2(int a, int b) {
    g = a + b;
    return 0;
}

struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 12, 3 };
    struct Point c = { 0, 0 };
    struct Point d = { 9, 30 };

    sink2((a = b).x, (c = d).y);
    return g;
}

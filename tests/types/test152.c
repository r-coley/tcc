struct Point {
    int x;
    int y;
};

typedef struct Point Point;

int main() {
    Point p = { 42, 1 };
    return p.x;
}

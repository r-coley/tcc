struct Point {
    int x;
    int y;
};

typedef struct Point Point;

int main() {
    Point p = { 2, 40 };

    return p.x + p.y;
}

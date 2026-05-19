struct Point {
    int x;
    int y;
};

typedef struct Point Point;
typedef Point *PointPtr;

int main() {
    Point p = { 42, 1 };
    PointPtr q;

    q = &p;
    return q->x;
}

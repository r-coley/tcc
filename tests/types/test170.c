struct Point {
    int x;
    int y;
};

typedef struct Point Point;

int main() {
    return sizeof(Point) + sizeof(Point *);
}

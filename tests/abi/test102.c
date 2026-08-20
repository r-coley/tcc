/* Return type: struct (by hidden pointer) */
typedef struct { int x; int y; } Point;
Point make_point(int x, int y) {
    Point p;
    p.x = x;
    p.y = y;
    return p;
}
int main() {
    Point p = make_point(20, 22);
    return p.x + p.y;
}

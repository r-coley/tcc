/* Regression: global struct array where struct contains pointer field.
   Indexed load of pointer field must use 8-byte load (ldr x0). */
typedef struct { int x; int y; } Point;

static Point points[4];

Point *get_point(int i) {
    return &points[i];
}

int main() {
    points[0].x = 10; points[0].y = 11;
    points[1].x = 12; points[1].y = 13;
    points[2].x = 20; points[2].y = 22;
    Point *p = get_point(2);
    return p->x + p->y;  /* 20 + 22 = 42 */
}

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect r = { .tl = { 1, 2 }, .br = { 3, 4 } };
    struct Rect *p;
    p = &r;

    p->tl.y = 41;
    return r.tl.x + r.tl.y;
}

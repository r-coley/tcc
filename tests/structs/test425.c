struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect r = { .tl = { 1, 2 }, .br = { 40, 2 } };
    struct Rect *p;
    p = &r;

    return p->br.x + p->br.y;
}

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect r = { .tl = { 1, 2 }, .br = { 42, 9 } };
    struct Rect *p;
    p = &r;

    return p->br.x;
}

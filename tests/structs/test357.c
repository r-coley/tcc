struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

struct Rect make_rect() {
    struct Rect r = { .tl = { 1, 2 }, .br = { .x = 39 } };
    return r;
}

int main() {
    struct Rect r;
    r = make_rect();
    return r.tl.x + r.tl.y + r.br.x + r.br.y;
}

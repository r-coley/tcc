struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

struct Rect make_rect() {
    struct Rect r = { .tl = { 1, 2 }, .br = { 40, 9 } };
    return r;
}

int main() {
    return make_rect().tl.y + make_rect().br.x;
}

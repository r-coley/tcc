struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect r = { .tl = { .x = 1, .y = 2 }, .br = { .x = 39 } };

    return r.tl.x + r.tl.y + r.br.x + r.br.y;
}

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int mutate_rect(struct Rect r) {
    r.tl.x = 100;
    return r.tl.x;
}

int main() {
    struct Rect r = { .tl = { 1, 2 }, .br = { .x = 39 } };
    mutate_rect(r);
    return r.tl.x + r.tl.y + r.br.x + r.br.y;
}

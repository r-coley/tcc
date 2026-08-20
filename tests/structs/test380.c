struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect a = { .tl = { 0, 0 }, .br = { 0, 0 } };
    struct Rect b = { .tl = { 0, 0 }, .br = { 0, 0 } };
    struct Rect c = { .tl = { 1, 2 }, .br = { 39, 0 } };

    a = b = c;

    return a.tl.x + a.tl.y + a.br.x + a.br.y;
}

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect a = { .tl = { 1, 2 }, .br = { .x = 39 } };
    struct Rect b;

    b = a;

    return b.tl.x + b.tl.y + b.br.x + b.br.y;
}

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect a = { .tl = { 1, 2 }, .br = { 39, 0 } };
    struct Rect b = { .tl = { .y = 2, .x = 1 }, .br = { .x = 39 } };

    if (a == b)
        return 42;

    return 0;
}

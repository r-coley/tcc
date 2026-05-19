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
    struct Rect b = { .tl = { 1, 2 }, .br = { 42, 9 } };

    return (a = b).br.x;
}

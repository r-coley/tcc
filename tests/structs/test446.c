struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect rs[2] = {
        { { 1, 2 }, { 3, 4 } },
        { { 5, 6 }, { 42, 9 } }
    };

    struct Rect *p;
    p = &rs[0];

    return p[1].br.x;
}

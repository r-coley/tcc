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
        [1] = { { 1, 2 }, { 42, 9 } },
        [0] = { { 3, 4 }, { 5, 6 } }
    };

    return rs[1].br.x;
}

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect rs[1] = {
        { { 1, 2 }, { 42, 9 } }
    };

    return rs[0].br.x;
}

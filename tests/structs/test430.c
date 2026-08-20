struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point tl;
    struct Point br;
};

int main() {
    struct Rect rs[1];

    rs[0].br.x = 42;
    rs[0].br.y = 9;

    return rs[0].br.x;
}
